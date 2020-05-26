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
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('root is searchable', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      sport: {
        prefix: 'sp',
        fields: {
          default: {
            type: 'object',
            properties: {
              rando: { type: 'text' }
            }
          }
        }
      }
    }
  })

  try {
    await client.set({
      $id: 'sp1',
      default: {
        rando: {
          en: 'jonko'
        }
      }
    })
    t.pass()
  } catch (e) {
    console.error(e)
    t.fail()
  }
})
