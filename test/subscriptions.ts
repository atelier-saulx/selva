import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import { wait } from './assertions'

let srv
test.before(async () => {
  srv = await start({
    port: 5051
  })
})

test.after(async () => {
  await srv.destroy()
})

test.serial('basic id based subscriptions', async t => {
  const client = connect({ port: 5051 })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } }
    },
    types: {
      yeshType: {
        fields: {
          yesh: { type: 'string' }
        }
      }
    }
  })

  t.plan(4)

  const observable = await client.observe({ $id: 'root', yesh: true })
  let o1counter = 0
  const sub = observable.subscribe(d => {
    if (o1counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, { yesh: '' })
    } else if (o1counter === 1) {
      // gets update event
      t.deepEqualIgnoreOrder(d, { yesh: 'so nice' })
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  const thing = await client.set({
    type: 'yeshType',
    yesh: 'extra nice'
  })

  let o2counter = 0
  const other = await client.observe({ $id: thing, $all: true })
  const sub2 = other.subscribe(d => {
    if (o2counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, {
        id: thing,
        type: 'yeshType',
        yesh: 'extra nice'
      })
    } else if (o2counter === 1) {
      // gets delete event
      t.is(d, null)
    } else {
      t.fail
    }
    o2counter++
  })

  await wait(1000 * 5)

  await client.set({
    $id: 'root',
    no: 'no event pls'
  })

  await client.set({
    $id: 'root',
    yesh: 'so nice'
  })

  await client.delete({
    $id: thing
  })

  await wait(1000 * 5)

  sub.unsubscribe()
  sub2.unsubscribe()

  await client.delete('root')
})

test.serial('using $field works', async t => {
  const client = connect({ port: 5051 })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' } }
    },
    types: {
      yeshType: {
        fields: {
          yesh: { type: 'string' }
        }
      }
    }
  })

  t.plan(2)

  const observable = await client.observe({
    $id: 'root',
    id: true,
    aliasedField: { $field: 'yesh' }
  })

  let o1counter = 0
  const sub = observable.subscribe(d => {
    if (o1counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, { id: 'root' })
    } else if (o1counter === 1) {
      // gets update event
      t.deepEqualIgnoreOrder(d, { id: 'root', aliasedField: 'so nice' })
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  await wait(1000 * 1)

  await client.set({
    $id: 'root',
    yesh: 'so nice'
  })

  await wait(1000 * 1)

  sub.unsubscribe()

  await client.delete('root')
})

test.serial('refs resolve and get tracked correctly', async t => {
  const client = connect({ port: 5051 })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      yeshType: {
        fields: {
          yesh: { type: 'string' },
          yeeesh: { type: 'string' }
        }
      }
    }
  })

  t.plan(2)

  const yesh = await client.set({
    type: 'yeshType',
    yesh: { $ref: 'yeeesh' }
  })

  await wait(1000 * 1)

  const observable = await client.observe({
    $id: yesh,
    id: true,
    yesh: true
  })

  let o1counter = 0
  const sub = observable.subscribe(d => {
    if (o1counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, { id: yesh })
    } else if (o1counter === 1) {
      // gets update event
      t.deepEqualIgnoreOrder(d, { id: yesh, yesh: 'siiick' })
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  await wait(1000 * 1)

  await client.set({
    $id: yesh,
    yeeesh: 'siiick'
  })

  await wait(1000 * 1)

  sub.unsubscribe()

  await client.delete('root')
})

test.serial('basic $inherit when ancestors change', async t => {
  const client = connect({ port: 5051 })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' } }
    },
    types: {
      yeshType: {
        fields: {
          yesh: { type: 'string' }
        }
      }
    }
  })

  t.plan(2)

  const thing = await client.set({
    type: 'yeshType'
  })

  const observable = await client.observe({
    $id: thing,
    id: true,
    yesh: { $inherit: true }
  })

  let o1counter = 0
  const sub = observable.subscribe(d => {
    if (o1counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, { id: thing, yesh: '' })
    } else if (o1counter === 1) {
      // gets update event
      t.deepEqualIgnoreOrder(d, { id: thing, yesh: 'so nice' })
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  await wait(1000 * 1)

  await client.set({
    type: 'yeshType',
    yesh: 'so nice',
    children: [thing]
  })

  await wait(1000 * 1)

  sub.unsubscribe()

  await client.delete('root')
})

test.serial(
  'subscription client side reconnection test -- no event if no changes',
  async t => {
    const server = await start({
      port: 5052
    })

    const client = connect({ port: 5052 })

    await client.updateSchema({
      languages: ['en', 'de', 'nl'],
      rootType: {
        fields: { yesh: { type: 'string' }, no: { type: 'string' } }
      },
      types: {
        yeshType: {
          fields: {
            yesh: { type: 'string' }
          }
        }
      }
    })

    t.plan(4)

    const observable = await client.observe({ $id: 'root', yesh: true })
    let o1counter = 0
    const sub = observable.subscribe(d => {
      if (o1counter === 0) {
        // gets start event
        t.deepEqualIgnoreOrder(d, { yesh: '' })
      } else if (o1counter === 1) {
        // reconnect event
        t.deepEqualIgnoreOrder(d, { yesh: '' })
      } else if (o1counter === 2) {
        // gets update event
        t.deepEqualIgnoreOrder(d, { yesh: 'so nice' })
      } else if (o1counter === 3) {
        t.deepEqualIgnoreOrder(d, { yesh: 'so nice!!!' })
      } else {
        // doesn't get any more events
        t.fail()
      }
      o1counter++
    })

    await wait(1000 * 5)
    // should get no event after reconnection
    ;(<any>client).redis.subscriptionManager.disconnect()
    await wait(1000 * 5)
    ;(<any>client).redis.subscriptionManager.attemptReconnect()
    await wait(1000 * 5)

    await client.set({
      $id: 'root',
      yesh: 'so nice'
    })

    await wait(1000 * 1)

    await client.set({
      $id: 'root',
      yesh: 'so nice!!!'
    })

    await wait(1000 * 1)
    sub.unsubscribe()

    await client.delete('root')
    await server.destroy()
  }
)

test.serial(
  'subscription client side reconnection test -- event if pending changes',
  async t => {
    const client = connect({ port: 5053 })

    const server = await start({
      port: 5053
    })

    await client.updateSchema({
      languages: ['en', 'de', 'nl'],
      rootType: {
        fields: { yesh: { type: 'string' }, no: { type: 'string' } }
      },
      types: {
        yeshType: {
          fields: {
            yesh: { type: 'string' }
          }
        }
      }
    })

    t.plan(2)

    const observable = await client.observe({ $id: 'root', yesh: true })
    let o1counter = 0
    const sub = observable.subscribe(d => {
      if (o1counter === 0) {
        // gets start event
        t.deepEqualIgnoreOrder(d, { yesh: '' })
      } else if (o1counter === 1) {
        // gets update event
        t.deepEqualIgnoreOrder(d, { yesh: 'so nice!!!' })
      } else {
        // doesn't get any more events
        t.fail()
      }
      o1counter++
    })

    await wait(1000 * 5)
    // should get no event after reconnection
    ;(<any>client).redis.subscriptionManager.disconnect()
    await wait(1000 * 5)

    await client.set({
      $id: 'root',
      yesh: 'so nice'
    })

    await wait(1000 * 1)

    await client.set({
      $id: 'root',
      yesh: 'so nice!!!'
    })

    await wait(1000 * 1)
    ;(<any>client).redis.subscriptionManager.attemptReconnect()
    await wait(1000 * 5)

    sub.unsubscribe()

    await client.delete('root')
    await server.destroy()
  }
)

test.serial(
  'subscription server side reconnection test -- event if pending changes',
  async t => {
    const client = connect({ port: 5054 })

    const server = await start({
      port: 5054
    })

    await client.updateSchema({
      languages: ['en', 'de', 'nl'],
      rootType: {
        fields: { yesh: { type: 'string' }, no: { type: 'string' } }
      },
      types: {
        yeshType: {
          fields: {
            yesh: { type: 'string' }
          }
        }
      }
    })

    t.plan(2)

    const observable = await client.observe({ $id: 'root', yesh: true })
    let o1counter = 0
    const sub = observable.subscribe(d => {
      if (o1counter === 0) {
        // gets start event
        t.deepEqualIgnoreOrder(d, { yesh: '' })
      } else if (o1counter === 1) {
        // gets update event
        t.deepEqualIgnoreOrder(d, { yesh: 'so nice!!!' })
      } else {
        // doesn't get any more events
        t.fail()
      }
      o1counter++
    })

    await wait(1000 * 5)
    // should get no event after reconnection
    server.closeSubscriptions()
    await wait(1000 * 5)

    await client.set({
      $id: 'root',
      yesh: 'so nice'
    })

    await wait(1000 * 1)

    await client.set({
      $id: 'root',
      yesh: 'so nice!!!'
    })

    await wait(1000 * 1)
    server.openSubscriptions()
    await wait(1000 * 5)

    sub.unsubscribe()

    await client.delete('root')
    await server.destroy()
  }
)
