import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let srv2
let port: number
let port2: number
test.before(async () => {
  port = await getPort()
  port2 = await getPort()
  srv = await start({
    port,
  })
  srv2 = await startOrigin({
    name: 'config',
    port: port2,
    registry: { port },
  })
})

test.after(async (t) => {
  await srv.destroy()
  await srv2.destroy()
  await t.connectionsAreEmpty()
})

test.serial('basic id based subscriptions', async (t) => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } },
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' },
        },
      },
    },
  })

  t.plan(4)

  const observable = client.observe({ $id: 'root', yesh: true })
  let o1counter = 0
  const sub = observable.subscribe((d) => {
    if (o1counter === 0) {
      // gets start event
      t.is(d.yesh, undefined)
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
    yesh: 'extra nice',
  })

  let o2counter = 0
  const other = client.observe({ $id: thing, $all: true, aliases: false })
  const sub2 = other.subscribe((d) => {
    if (o2counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, {
        id: thing,
        type: 'yeshType',
        yesh: 'extra nice',
      })
    } else if (o2counter === 1) {
      // gets delete event
      t.true(d.$isNull)
    } else {
      t.fail()
    }
    o2counter++
  })

  await wait(500 * 2)

  await client.set({
    $id: 'root',
    no: 'no event pls',
  })

  await client.set({
    $id: 'root',
    yesh: 'so nice',
  })

  await client.delete({
    $id: thing,
  })

  await wait(500 * 2)

  sub.unsubscribe()
  sub2.unsubscribe()

  await wait(500 * 2)

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})

test.serial('basic id based nested query subscriptions', async (t) => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } },
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' },
        },
      },
    },
  })

  t.plan(2)

  const thing = await client.set({
    type: 'yeshType',
    yesh: 'extra nice',
  })

  let o2counter = 0
  const other = client.observe({
    $id: 'root',
    item: { $id: thing, $all: true, aliases: false },
  })
  const sub2 = other.subscribe((d) => {
    if (o2counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, {
        item: {
          id: thing,
          type: 'yeshType',
          yesh: 'extra nice',
        },
      })
    } else if (o2counter === 1) {
      console.log('DD', d)
      // gets delete event
      t.deepEqualIgnoreOrder(d, {})
    } else {
      t.fail()
    }
    o2counter++
  })

  await wait(500 * 2)

  await client.set({
    $id: 'root',
    no: 'no event pls',
  })

  await client.set({
    $id: 'root',
    yesh: 'so nice',
  })

  await client.delete({
    $id: thing,
  })

  await wait(500 * 2)

  sub2.unsubscribe()

  await wait(500 * 2)

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})

test.serial('using $field works', async (t) => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' } },
    },
    types: {
      yeshType: {
        fields: {
          yesh: { type: 'string' },
        },
      },
    },
  })

  t.plan(2)

  const observable = client.observe({
    $id: 'root',
    id: true,
    aliasedField: { $field: 'yesh' },
  })

  let o1counter = 0
  const sub = observable.subscribe((d) => {
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
    yesh: 'so nice',
  })

  await wait(1000 * 1)

  sub.unsubscribe()

  await client.delete('root')

  await client.destroy()
})

test.serial.skip('refs resolve and get tracked correctly', async (t) => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      yeshType: {
        fields: {
          yesh: { type: 'string' },
          yeeesh: { type: 'string' },
        },
      },
    },
  })

  t.plan(2)

  const yesh = await client.set({
    type: 'yeshType',
    yesh: { $ref: 'yeeesh' },
  })

  await wait(1000 * 1)

  const observable = client.observe({
    $id: yesh,
    id: true,
    yesh: true,
  })

  let o1counter = 0
  const sub = observable.subscribe((d) => {
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
    yeeesh: 'siiick',
  })

  await wait(1000 * 1)

  sub.unsubscribe()

  await client.delete('root')
  await client.destroy()
})

test.serial('basic $inherit when ancestors change', async (t) => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' } },
    },
    types: {
      yeshType: {
        fields: {
          yesh: { type: 'string' },
        },
      },
    },
  })

  t.plan(2)

  const thing = await client.set({
    type: 'yeshType',
  })

  const observable = client.observe({
    $id: thing,
    id: true,
    yesh: { $inherit: { $type: ['yeshType', 'root'] } },
  })

  let o1counter = 0
  const sub = observable.subscribe((d) => {
    if (o1counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, { id: thing })
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
    children: [thing],
  })

  await wait(1000 * 1)

  sub.unsubscribe()

  await client.delete('root')
  await client.destroy()
})

test.serial(
  'subscription client side reconnection test -- no event if no changes',
  async (t) => {
    const port = await getPort()
    const server = await start({
      port,
    })

    const client = connect({ port })

    await client.updateSchema({
      languages: ['en', 'de', 'nl'],
      rootType: {
        fields: { yesh: { type: 'string' }, no: { type: 'string' } },
      },
      types: {
        yeshType: {
          fields: {
            yesh: { type: 'string' },
          },
        },
      },
    })

    t.plan(3)

    const observable = client.observe({ $id: 'root', yesh: true })
    let o1counter = 0
    const sub = observable.subscribe((d) => {
      if (o1counter === 0) {
        // gets start event
        t.deepEqualIgnoreOrder(d, {})
      } else if (o1counter === 1) {
        // reconnect event
        t.deepEqualIgnoreOrder(d, { yesh: 'so nice' })
      } else if (o1counter === 2) {
        t.deepEqualIgnoreOrder(d, { yesh: 'so nice!!!' })
      } else {
        // doesn't get any more events
        t.fail()
      }
      o1counter++
    })

    await wait(1000)
    // should get no event after reconnection
    // client.redis.disconnect()
    // await wait(1000 * 5)
    // client.redis.redis.reconnect()
    // await wait(1000 * 5)

    await client.set({
      $id: 'root',
      yesh: 'so nice',
    })

    await wait(1000 * 1)

    await client.set({
      $id: 'root',
      yesh: 'so nice!!!',
    })

    await wait(1000 * 1)
    sub.unsubscribe()

    await client.delete('root')
    await server.destroy()
    await client.destroy()
  }
)

// still have to fix this
test.serial(
  'subscription client side reconnection test -- event if pending changes',
  async (t) => {
    const port = await getPort()
    const client = connect({ port })

    const server = await start({
      port,
    })

    await client.updateSchema({
      languages: ['en', 'de', 'nl'],
      rootType: {
        fields: { yesh: { type: 'string' }, no: { type: 'string' } },
      },
      types: {
        yeshType: {
          fields: {
            yesh: { type: 'string' },
          },
        },
      },
    })

    t.plan(3)

    const observable = client.observe({ $id: 'root', yesh: true })
    let o1counter = 0
    const sub = observable.subscribe((d) => {
      if (o1counter === 0) {
        // gets start event
        t.deepEqualIgnoreOrder(d, {})
      } else if (o1counter === 1) {
        // gets update event
        t.deepEqualIgnoreOrder(d, { yesh: 'so nice' })
      } else if (o1counter === 2) {
        t.deepEqualIgnoreOrder(d, { yesh: 'so nice!!!' })
      } else {
        // doesn't get any more events
        t.fail()
      }
      o1counter++
    })

    await wait(1000)

    // should get no event after reconnection
    // client.redis.redis.disconnect()
    // await wait(1000 * 5)
    // client.redis.redis.reconnect()

    await client.set({
      $id: 'root',
      yesh: 'so nice',
    })

    await wait(1000 * 5)

    // client.redis.redis.reconnect()

    await client.set({
      $id: 'root',
      yesh: 'so nice!!!',
    })

    await wait(1000 * 5)

    sub.unsubscribe()

    await client.delete('root')
    await server.destroy()
    await client.destroy()

    // somethign fishy here
  }
)

// skip for now
test.serial.skip(
  'subscription server side reconnection test -- event if pending changes',
  async (t) => {
    const port = await getPort()
    const client = connect({ port })

    const server = await start({
      port,
    })

    await client.updateSchema({
      languages: ['en', 'de', 'nl'],
      rootType: {
        fields: { yesh: { type: 'string' }, no: { type: 'string' } },
      },
      types: {
        yeshType: {
          fields: {
            yesh: { type: 'string' },
          },
        },
      },
    })

    t.plan(2)

    const observable = client.observe({ $id: 'root', yesh: true })
    let o1counter = 0

    const sub = observable.subscribe((d) => {
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
    // TODO
    // server.closeSubscriptions()
    await wait(1000 * 5)

    await client.set({
      $id: 'root',
      yesh: 'so nice',
    })

    await wait(1000 * 1)

    await client.set({
      $id: 'root',
      yesh: 'so nice!!!',
    })

    await wait(1000 * 1)

    // TODO
    // server.openSubscriptions()
    await wait(1000 * 5)

    sub.unsubscribe()

    await client.delete('root')
    await server.destroy()
    await client.destroy()
  }
)

test.serial('basic id based reference subscriptions', async (t) => {
  const client = connect({ port })

  await client.updateSchema(
    {
      languages: ['en', 'de', 'nl'],
      rootType: {
        fields: { yesh: { type: 'string' }, no: { type: 'string' } },
      },
      types: {
        refType: {
          prefix: 're',
          fields: {
            yesh: { type: 'string' },
            myRef: { type: 'reference' },
          },
        },
      },
    },
    'config'
  )

  t.plan(3)

  await client.set({
    $db: 'config',
    $id: 're2',
    yesh: 'hello from 2',
  })

  await client.set({
    $db: 'config',
    $id: 're1',
    yesh: 'hello from 1',
    myRef: 're2',
  })

  const observable = client.observe({
    $db: 'config',
    $id: 're1',
    yesh: true,
    myRef: {
      yesh: true,
    },
  })

  let o1counter = 0
  const sub = observable.subscribe((d) => {
    console.log('ddd', d)
    if (o1counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, {
        yesh: 'hello from 1',
        myRef: { yesh: 'hello from 2' },
      })
    } else if (o1counter === 1) {
      // gets update event
      t.deepEqualIgnoreOrder(d, {
        yesh: 'hello from 1!',
        myRef: { yesh: 'hello from 2' },
      })
    } else if (o1counter === 2) {
      t.deepEqualIgnoreOrder(d, {
        yesh: 'hello from 1!',
        myRef: { yesh: 'hello from 2!' },
      })
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  await wait(500 * 2)

  await client.set({
    $db: 'config',
    $id: 're1',
    yesh: 'hello from 1!',
  })

  await wait(500 * 2)

  await client.set({
    $db: 'config',
    $id: 're2',
    yesh: 'hello from 2!',
  })

  await wait(500 * 2)

  sub.unsubscribe()

  await wait(500 * 2)

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})
