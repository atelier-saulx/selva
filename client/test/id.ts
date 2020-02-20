import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'

import getPort from 'get-port'

test('generates a unique id from type', async t => {
  const port = await getPort()
  const server = await start({ port })

  const client = connect({
    port
  })

  await client.updateSchema({
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text'
          }
        }
      }
    }
  })

  const id1 = await client.id({ type: 'match' })

  const id2 = await client.id({ type: 'match' })
  t.true(id1 !== id2)
  t.true(/ma.+/.test(id1))
  // new types what this means is that the client allways needs to load a map add it to prefix
  // allways subscribe on it (hash)

  server.destroy()
})
