import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import './assertions'
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

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
  await srv2.destroy()
})

test.serial('inherit references $list', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      blurf: {
        prefix: 'bl',
        fields: {
          rando: { type: 'string' }
        }
      }
    }
  })

  let cnt = 50000
  const promises = []
  while (cnt--) {
    promises.push(client.set({ type: 'blurf', rando: 'ballz' + cnt }))
  }

  // client
  //   .observe({
  //     $db: 'snurk',
  //     children: {
  //       id: true,
  //       $list: true
  //     }
  //   })
  //   .subscribe(res => {
  //     console.log('HIEEEER', res)
  //   })

  await wait(3000)

  // await client.updateSchema(
  //   {
  //     languages: ['en'],
  //     types: {
  //       blurf: {
  //         prefix: 'bl',
  //         fields: {
  //           rando: { type: 'string' }
  //         }
  //       }
  //     }
  //   },
  //   'snurk'
  // )

  // client.set({
  //   $db: 'snurk',
  //   type: 'blurf',
  //   rando: 'amaze'
  // })

  // await wait(1000)

  // t.deepEqualIgnoreOrder(res, { $isNull: true, menu: [] })
})
