import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async () => {
  port = await getPort()
  srv = await start({
    port
  })
})

test.after(async t => {
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('basic trigger created subscriptions', async t => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' }
        }
      },
      noType: {
        prefix: 'no',
        fields: {
          no: { type: 'string' }
        }
      }
    }
  })

  await client.set({ $id: 'root' })

  t.plan(2)

  let o2counter = 0
  const other = client.observeEvent('created', {
    $filter: {
      $operator: '=',
      $field: 'type',
      $value: 'yeshType'
    },
    $all: true,
    aliases: false
  })

  const sub2 = other.subscribe(d => {
    if (o2counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, {
        id: thing,
        type: 'yeshType',
        yesh: 'extra nice'
      })
    } else if (o2counter === 1) {
      // gets start event
      t.deepEqualIgnoreOrder(d, {
        id: thing2,
        type: 'yeshType',
        yesh: 'extra extra nice'
      })
    } else {
      t.fail
    }
    o2counter++
  })

  await wait(500)

  const thing = await client.set({
    type: 'yeshType',
    yesh: 'extra nice'
  })

  await wait(500)

  const thing2 = await client.set({
    type: 'yeshType',
    yesh: 'extra extra nice'
  })

  await wait(500)

  try {
    const subs = await client.redis.selva_subscriptions_list(
      '___selva_hierarchy'
    )

    const sub = subs[0]
    console.log(
      await client.redis.selva_subscriptions_debug('___selva_hierarchy', sub)
    )
  } catch (e) {
    console.log('wtf', e)
  }

  await wait(500 * 2)

  await client.set({
    $id: 'root',
    no: 'no event pls'
  })

  await client.set({
    $id: 'noNoNoNo',
    no: 'no event again'
  })

  await client.set({
    $id: thing,
    yesh: 'woot no event'
  })

  await client.set({
    $id: thing2,
    yesh: 'woot no event'
  })

  // no event
  await client.delete({
    $id: thing
  })

  await wait(500 * 2)

  sub2.unsubscribe()

  await wait(500 * 2)

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})

test.serial('basic trigger updated subscriptions', async t => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' }
        }
      },
      noType: {
        prefix: 'no',
        fields: {
          no: { type: 'string' }
        }
      }
    }
  })

  await client.set({ $id: 'root' })
  const thing = await client.set({
    type: 'yeshType',
    yesh: 'nice'
  })

  const thing2 = await client.set({
    type: 'noType',
    no: 'initial value'
  })

  t.plan(2)

  let o2counter = 0
  const other = client.observeEvent('updated', {
    $filter: {
      $operator: '=',
      $field: 'type',
      $value: 'yeshType',
      $or: {
        $operator: '=',
        $field: 'type',
        $value: 'noType'
      }
    },
    $all: true,
    aliases: false
  })

  const sub2 = other.subscribe(d => {
    if (o2counter === 0) {
      // gets start event
      t.deepEqualIgnoreOrder(d, {
        id: thing,
        type: 'yeshType',
        yesh: 'extra nice'
      })
    } else if (o2counter === 1) {
      // gets start event
      t.deepEqualIgnoreOrder(d, {
        id: thing2,
        type: 'noType',
        no: 'hmm'
      })
    } else {
      t.fail
    }
    o2counter++
  })

  await wait(500)

  await client.set({
    type: 'yeshType',
    yesh: 'new node, no event'
  })

  await client.set({
    type: 'noType',
    no: 'new node, no event'
  })

  await client.set({
    $id: thing,
    yesh: 'extra nice'
  })

  await wait(100)
  await client.set({
    $id: thing2,
    no: 'hmm'
  })

  try {
    const subs = await client.redis.selva_subscriptions_list(
      '___selva_hierarchy'
    )

    const sub = subs[0]
    console.log(
      await client.redis.selva_subscriptions_debug('___selva_hierarchy', sub)
    )
  } catch (e) {
    console.log('wtf', e)
  }

  await wait(500 * 2)

  await client.set({
    $id: 'root',
    no: 'no event pls'
  })

  await client.set({
    $id: 'noNoNoNo',
    no: 'no event again'
  })

  // no event
  await client.delete({
    $id: thing
  })

  await wait(500 * 2)

  sub2.unsubscribe()

  await wait(500 * 2)

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})

test.serial('basic trigger deleted subscriptions', async t => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' }
        }
      },
      noType: {
        prefix: 'no',
        fields: {
          no: { type: 'string' }
        }
      }
    }
  })

  await client.set({ $id: 'root' })
  const thing = await client.set({
    type: 'yeshType',
    yesh: 'nice'
  })

  const thing2 = await client.set({
    type: 'noType',
    no: 'initial value'
  })

  t.plan(1)

  let o2counter = 0
  const other = client.observeEvent('deleted', {
    $filter: {
      $operator: '=',
      $field: 'type',
      $value: 'yeshType',
      $or: {
        $operator: '=',
        $field: 'type',
        $value: 'noType'
      }
    },
    $all: true,
    aliases: false
  })

  const sub2 = other.subscribe(d => {
    if (o2counter === 0) {
      // yes
      console.log('d', d)
      t.pass()
    } else {
      t.fail
    }
    o2counter++
  })

  await wait(500)

  await client.set({
    type: 'yeshType',
    yesh: 'new node, no event'
  })

  await client.set({
    type: 'noType',
    no: 'new node, no event'
  })

  await client.set({
    $id: thing,
    yesh: 'extra nice'
  })

  await client.set({
    $id: thing2,
    no: 'hmm'
  })

  try {
    const subs = await client.redis.selva_subscriptions_list(
      '___selva_hierarchy'
    )

    const sub = subs[0]
    console.log(
      await client.redis.selva_subscriptions_debug('___selva_hierarchy', sub)
    )
  } catch (e) {
    console.log('wtf', e)
  }

  await wait(500 * 2)

  await client.set({
    $id: 'root',
    no: 'no event pls'
  })

  await client.set({
    $id: 'noNoNoNo',
    no: 'no event again'
  })

  // no event
  await client.delete({
    $id: thing
  })

  await wait(500 * 2)

  sub2.unsubscribe()

  await wait(500 * 2)

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})
