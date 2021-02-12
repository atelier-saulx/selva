import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async () => {
  port = await getPort()
  srv = await start({
    port,
  })
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('exec big batch', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      blurf: {
        prefix: 'bl',
        fields: {
          rando: { type: 'string' },
        },
      },
    },
  })

  let cnt = 50000
  const promises = []
  while (cnt--) {
    promises.push(client.set({ type: 'blurf', rando: 'ballz' + cnt }))
  }

  t.timeout(120000)
  try {
    await Promise.all(promises)
    t.pass()
  } catch (e) {
    console.error(e)
    t.fail()
  }

  await client.destroy()
})
