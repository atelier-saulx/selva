import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { wait, dumpDb } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6089,
    developmentLogging: true,
    loglevel: 'info'
  })

  await wait(500)

  const client = connect({ port: 6089 })
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
      season: {
        prefix: 'se',
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
  const client = connect({ port: 6089 })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('find - ancestors', async t => {
  // simple nested - single query
  const client = connect({ port: 6089 })

  const teams = []
  for (let i = 0; i < 11; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      type: 'team',
      name: 'team' + i
    })
  }

  const globMatches = []
  const leagues = []
  for (let i = 0; i < 10; i++) {
    const matches = []
    for (let j = 0; j < 10; i++) {
      const match = {
        $id: await client.id({ type: 'match' }),
        type: 'match',
        name: 'match' + j,
        parents: { $add: [teams[i], teams[i + 1]] }
      }
      matches.push(match)
      globMatches.push(match)
    }
    leagues.push({
      type: 'league',
      name: 'league' + i,
      children: [
        {
          type: 'season',
          name: 'season1-' + i,
          children: [...teams, ...matches]
        }
      ]
    })
  }

  await Promise.all([...teams, ...leagues].map(v => client.set(v)))

  const all = (await dumpDb(client)).filter(v => {
    if (typeof v[1] === 'object') {
      return true
    }
    return false
  })

  console.log(all)

  let d = Date.now()
  const results = await client.query({
    $id: teams[0],
    name: true,
    value: true,
    status: true,
    date: true,
    id: true,
    type: true,
    $list: {
      $sort: { $field: 'status', $order: 'desc' },
      $find: {
        $traverse: 'ancestors',
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
