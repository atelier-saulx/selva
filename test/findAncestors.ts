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
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
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
    for (let j = 0; j < 10; j++) {
      const match = {
        $id: await client.id({ type: 'match' }),
        type: 'match',
        name: 'match' + j,
        parents: { $add: [teams[i].$id, teams[i + 1].$id] }
      }
      matches.push(match)
      globMatches.push(match)
    }
    leagues.push({
      type: 'league',
      name: 'league' + i,
      value: i,
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

  // needs an array
  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: teams[0].$id,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'ancestors',
              $filter: [
                // special case does not traverse
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league'
                }
              ]
            }
          }
        }
      })
    ).map(v => v.name),
    [
      { name: 'league1' },
      { name: 'league7' },
      { name: 'league9' },
      { name: 'league0' },
      { name: 'league4' },
      { name: 'league6' },
      { name: 'league5' },
      { name: 'league2' },
      { name: 'league3' },
      { name: 'league8' }
    ].map(v => v.name),
    'find ancestors without redis search TYPE'
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: globMatches[0].$id,
        name: true,
        $list: {
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: ['season', 'team', 'league']
              }
            ]
          }
        }
      })
    ).map(v => v.name),
    [
      { name: 'league0' },
      { name: 'season1-0' },
      { name: 'team0' },
      { name: 'team1' }
    ].map(v => v.name),
    'find ancestors without redis search TYPE or'
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: globMatches[0].$id,
        name: true,
        id: true,
        $list: {
          $find: {
            $traverse: 'ancestors'
          }
        }
      })
    ).map(v => v.name || v.id),
    ['league0', 'season1-0', 'team0', 'team1', 'root'],
    'find ancestors without redis search and without filters'
  )

  const r = await client.get({
    $id: teams[0].$id,
    name: true,
    $list: {
      $find: {
        $traverse: 'ancestors',
        $filter: [
          {
            $field: 'type',
            $operator: '=',
            $value: 'league'
          },
          {
            $field: 'value',
            $operator: '..',
            $value: [2, 4]
          }
        ]
      }
    }
  })

  t.deepEqualIgnoreOrder(
    r.map(v => v.name),
    ['league2', 'league3', 'league4'],
    'find ancestors redis search'
  )
})
