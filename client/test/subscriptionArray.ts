import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  const client = connect({ port })
  await client.updateSchema({
    types: {
      match: {
        prefix: 'ma',
        fields: {
          results: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                id: { type: 'reference' },
                score: { type: 'int' },
                points: { type: 'int' }
              }
            }
          }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial.skip('subscribe array', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'ma1',
    results: [
      { score: 11, points: 1 },
      { score: 10, points: 111 }
    ]
  })

  await wait(500)

  client
    .observe({
      $id: 'ma1',
      $all: true
    })
    .subscribe(res => {
      console.log('--->', res)
    })

  await wait(500)

  await client.set({
    $id: 'ma1',
    results: [
      { score: 553, points: 1 },
      { score: 10, points: 111 }
    ]
  })

  await wait(500)

  await client.set({
    $id: 'ma1',
    results: [
      {
        score: 1,
        points: 1
      },
      {
        score: 2,
        points: 2
      }
    ]
  })

  await wait(1000)
  // const res = await client.get({
  //   $id: 'root'
  // })
  // t.deepEqualIgnoreOrder(res, { $isNull: true, menu: [] })
})
