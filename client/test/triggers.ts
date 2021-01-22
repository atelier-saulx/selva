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

test.serial('basic trigger subscriptions', async t => {
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
      // gets delete event
      t.true(d.$isNull)
    } else {
      t.fail
    }
    o2counter++
  })

  const thing = await client.set({
    type: 'yeshType',
    yesh: 'extra nice'
  })

  try {
    console.log(
      await client.redis.selva_subscriptions_list('___selva_hierarchy')
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
