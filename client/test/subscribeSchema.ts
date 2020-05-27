import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let srv2
let port: number
test.before(async () => {
  port = await getPort()
  srv = await start({
    port
  })
  srv2 = await startOrigin({
    registry: { port },
    name: 'snurk'
  })
  console.log('ok server started!')
})

test.after(async () => {
  await srv.destroy()
})

test.serial('basic schema based subscriptions', async t => {
  const client = connect({ port })

  const obssnurk = client.subscribeSchema('snurk')

  let snurkCnt = 0
  obssnurk.subscribe(x => {
    snurkCnt++
    console.log('SNURK', x.rootType)
    if (snurkCnt === 2) {
      console.log(x.rootType.fields)
      if (!x.rootType.fields.snurk) {
        throw new Error('does not have snurk!')
      }
    }
  })
  await wait(2000)

  console.log('----------------------------------')

  console.log('update snurk')
  await client.updateSchema(
    {
      languages: ['en', 'de', 'nl'],
      rootType: {
        fields: { snurk: { type: 'string' } }
      }
    },
    'snurk'
  )

  console.log('snurk updated')

  const observable = client.subscribeSchema()
  let o1counter = 0
  const sub = observable.subscribe(d => {
    o1counter++
  })

  await wait(2000)

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } }
    }
  })

  await wait(500)

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

  await wait(500)

  console.log('----------------------------------')
  console.log('unsubscribe')

  sub.unsubscribe()

  await wait(500)
  t.is(o1counter, 3)
  console.log('----------------------------------')
  console.log('best')
  const observable2 = client.subscribeSchema()
  var cnt = 0
  const sub2 = observable2.subscribe(d => {
    cnt++
  })

  await wait(500)

  t.is(cnt, 1)

  t.is(snurkCnt, 2)

  sub2.unsubscribe()

  await wait(1500)
})
