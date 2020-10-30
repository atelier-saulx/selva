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
  const theme = {
    type: 'object',
    properties: {
      colors: {
        type: 'object',
        properties: {
          blue: { type: 'string' }
        }
      }
    }
  }

  await client.updateSchema({
    rootType: {
      fields: {
        // @ts-ignore
        theme
      }
    },
    types: {
      team: {
        prefix: 'te',
        fields: {
          // @ts-ignore
          theme
        }
      },

      match: {
        prefix: 'ma',
        hierarchy: {
          team: false
        },
        fields: {
          // @ts-ignore
          theme
        }
      }
    }
  })

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('inherit even when skipping hierarchy node', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'root',
    theme: {
      colors: {
        blue: 'red'
      }
    }
  })

  const team = await client.set({
    type: 'team'
  })

  const match = await client.set({
    type: 'match',
    parents: [team]
  })

  const res = await client.get({
    $id: match,
    theme: { colors: { $inherit: { $type: ['match', 'team', 'root'] } } }
  })

  t.deepEqualIgnoreOrder(res, { theme: { colors: { blue: 'red' } } })

  await client.destroy()
})
