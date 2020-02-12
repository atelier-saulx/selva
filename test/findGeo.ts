import test from 'ava'
import { connect } from '../client/src/index'
import { start } from '../server/src/index'
import './assertions'
import { wait } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6099
  })

  await wait(500)

  const client = connect({ port: 6099 })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          fun: { type: 'set', items: { type: 'string' } },
          location: { type: 'geo', search: true },
          related: { type: 'references', search: { type: ['TAG'] } },
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port: 6099 })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('find - geo', async t => {
  // simple nested - single query
  const client = connect({ port: 6099 }, { loglevel: 'info' })
  const ma1 = await client.set({
    type: 'match',
    name: 'match 1',
    location: {
      lat: 60.12,
      lon: 120.67
    }
  })

  console.log(
    await client.get({
      $id: ma1,
      $all: true
    })
  )
  t.true(true)
})
