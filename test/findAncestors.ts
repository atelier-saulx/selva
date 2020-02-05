import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { wait, dumpDb } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6088,
    developmentLogging: true,
    loglevel: 'info'
  })

  await wait(500)

  const client = connect({ port: 6088 })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      club: {
        prefix: 'cl',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      match: {
        hierarchy: {
          team: { excludeAncestryWith: ['league'] }
        },
        prefix: 'ma',
        fields: {
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
  const client = connect({ port: 6088 })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('find - ancestors', async t => {
  // simple nested - single query
  const client = connect({ port: 6088 })

  // extra option in find is index or auto from fields
  let d = Date.now()
  const results = await client.query({
    name: true,
    value: true,
    status: true,
    date: true,
    id: true,
    type: true,
    $list: {
      $sort: { $field: 'status', $order: 'desc' },
      $find: {
        $traverse: 'descendants',
        $filter: []
      }
    }
  })

  console.log('Executing query (1100 resuls)', Date.now() - d, 'ms')

  // const matches = results.filter(v => v.type === 'match')
  // const videos = results.filter(v => v.type === 'video')
  // const league = results.filter(v => v.type === 'league')
  // t.is(matches.length, 997, 'query result matches')
  // t.is(videos.length, 3, 'query result videos')
  // t.is(league.length, 1, 'query result league')
})
