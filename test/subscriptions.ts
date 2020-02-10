import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import { wait } from './assertions'

test.serial('basic id based subscriptions', async t => {
  const client = connect({ port: 5051 })

  const server = await start({
    port: 5051,
    loglevel: 'info',
    developmentLogging: true
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

  server.destroy()
})
