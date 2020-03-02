import test from 'ava'
import { connect } from '@saulx/selva'
import { start, SelvaServer } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port' 

let srv:SelvaServer
let port

test.before(async t => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port: port })
  await client.updateSchema({
    types: {
      genre: {
        prefix: 'ge',
        fields: {
          name: { type: 'string' },
          icon: { type: 'string' }
        }
      },
      movie: {
        prefix: 'mo',
        fields: {
          title: { type: 'string' },
          icon: { type: 'string' }
        }
      }
    }
  })
  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('$list', async t => {
  const client = connect({ port: port })

  const genre = await client.set({
    $id: 'geScifi',
    name: 'Sci-fi',
    icon: 'scifi.png'
  })

  await client.set({
    $id: 'moSoylentGreen',
    title: 'Soylent Green',
    parents: [genre]
  })

  const result = await client.get({
    $id: 'moSoylentGreen',
    icon: { $inherit: true }
  })
  console.log(result)

  t.true(result.icon === 'scifi.png')
})
