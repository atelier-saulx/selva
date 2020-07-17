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
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})
