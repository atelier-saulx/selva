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
  console.log('ok server started!')
})

test.after(async () => {
  await srv.destroy()
})

test.serial('basic schema based subscriptions', async t => {
  const client = connect({ port })

  t.plan(3)

  const observable = client.subscribeSchema()
  let o1counter = 0
  const sub = observable.subscribe(d => {
    if (o1counter === 0) {
      // gets update event
      t.deepEqualIgnoreOrder(d.types, {})
      const rootKeys = Object.keys(d.rootType.fields)
      t.assert(['yesh', 'no'].every(k => rootKeys.includes(k)))
    } else if (o1counter === 1) {
      const keys = Object.keys(d.types.yeshType.fields)
      t.assert(['yesh'].every(k => keys.includes(k)))
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } }
    }
  })

  await wait(500 * 2)

  console.log('----------------------------------')
  console.log('set some things')
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

  await wait(500 * 2)

  console.log('----------------------------------')
  console.log('unsubscribe')

  sub.unsubscribe()

  await wait(500 * 2)

  await wait(1000)
})
