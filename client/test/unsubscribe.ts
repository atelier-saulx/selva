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

test.serial('unsubscribe removes subscriptions from redis', async t => {
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
      }
    }
  })

  t.plan(5)

  const observable = client.observe({ $id: 'root', yesh: true })
  let o1counter = 0
  const sub = observable.subscribe(d => {
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
    yesh: 'extra nice'
  })

  let o2counter = 0
  const other = client.observe({ $id: thing, $all: true, aliases: false })
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

  await wait(500 * 2)

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

  await wait(500 * 2)

  sub.unsubscribe()
  sub2.unsubscribe()

  await wait(500 * 2)

  const subs = await client.redis.selva_subscriptions_list('___selva_hierarchy')

  t.deepEqualIgnoreOrder(
    subs.length,
    0,
    'there should be no subs after unsubscribing'
  )

  await wait(500 * 2)

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})
