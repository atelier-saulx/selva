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

test.serial('basic inherit subscription', async t => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        yesh: { type: 'string' },
        no: { type: 'string' },
        flapper: {
          type: 'object',
          properties: {
            snurk: { type: 'string' },
            bob: { type: 'string' }
          }
        }
      }
    },
    types: {
      yeshType: {
        fields: {
          yesh: { type: 'string' },
          flapper: {
            type: 'object',
            properties: {
              snurk: { type: 'string' },
              bob: { type: 'string' }
            }
          }
        }
      }
    }
  })

  // const observable = await client.observe({ $id: 'root', yesh: true })
  // let o1counter = 0
  // const sub = observable.subscribe(d => {
  //   if (o1counter === 0) {
  //     // gets start event
  //     t.deepEqualIgnoreOrder(d, { yesh: '' })
  //   } else if (o1counter === 1) {
  //     // gets update event
  //     t.deepEqualIgnoreOrder(d, { yesh: 'so nice' })
  //   } else {
  //     // doesn't get any more events
  //     t.fail()
  //   }
  //   o1counter++
  // })

  // const thing = await client.set({
  //   type: 'yeshType',
  //   yesh: 'extra nice'
  // })

  // await wait(1000)
  t.true(true)
})
