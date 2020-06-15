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
      sport: {
        prefix: 'sp',
        fields: {
          // @ts-ignore
          theme
        }
      },

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
          team: {
            excludeAncestryWith: ['sport']
          }
        },
        // hierarchy: {
        //   team: false
        // },
        fields: {
          // @ts-ignore
          theme
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port }, { loglevel: 'info' })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
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

  let count = 10000
  let match
  while (count--) {
    const sport = await client.set({
      type: 'sport'
    })

    const team = await client.set({
      type: 'team',
      parents: [sport]
    })

    match = await client.set({
      $id: 'ma' + count,
      type: 'match',
      parents: [team]
    })
  }

  count = 1000
  while (count--) {
    const res = await client.get({
      $id: 'ma' + count,
      theme: { colors: { $inherit: true } }
    })
    t.deepEqualIgnoreOrder(res, { theme: { colors: { blue: 'red' } } })
  }
})
