import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait, dumpDb } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })

  await wait(500)
})

test.beforeEach(async t => {
  const client = connect({ port })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      region: {
        prefix: 're',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      },
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
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
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      }
    }
  })

  // A small delay is needed after setting the schema
  await new Promise(r => setTimeout(r, 100))

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial.only('find - ancestors', async t => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })

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
    ).items.map(v => v.name),
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
        items: {
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
        }
      })
    ).items.map(v => v.name),
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
        items: {
          name: true,
          id: true,
          $list: {
            $find: {
              $traverse: 'ancestors'
            }
          }
        }
      })
    ).items.map(v => v.name || v.id),
    ['league0', 'season1-0', 'team0', 'team1', 'root'],
    'find ancestors without redis search and without filters'
  )

  const r = await client.get({
    $id: teams[0].$id,
    items: {
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
    }
  })

  t.deepEqualIgnoreOrder(
    r.items.map(v => v.name),
    ['league2', 'league3', 'league4'],
    'find ancestors redis search'
  )
})

test.serial.skip('find - ancestors - regions', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const teams = []

  const regions = await Promise.all([
    client.set({
      type: 'region',
      name: 'REGION De'
    }),
    client.set({
      type: 'region',
      name: 'REGION Nl'
    })
  ])

  for (let i = 0; i < 11; i++) {
    await client.set({
      type: 'team',
      name: 'team region ' + i,
      parents: {
        $add: i < 6 ? regions[0] : regions[1]
      }
    })
  }

  const dutchteams = await client.get({
    teams: {
      name: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'ancestors',
              $operator: '=',
              $value: regions[0]
            },
            {
              $field: 'type',
              $operator: '=',
              $value: 'team'
            }
          ]
        }
      }
    }
  })

  t.deepEqualIgnoreOrder(
    dutchteams,
    {
      teams: [
        { name: 'team region 5' },
        { name: 'team region 4' },
        { name: 'team region 3' },
        { name: 'team region 2' },
        { name: 'team region 1' },
        { name: 'team region 0' }
      ]
    },
    'dutch teams'
  )
})

test.serial.skip('find - ancestors - regions - no wrapping', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const teams = []

  const regions = await Promise.all([
    client.set({
      type: 'region',
      name: 'REGION De'
    }),
    client.set({
      type: 'region',
      name: 'REGION Nl'
    })
  ])

  for (let i = 0; i < 11; i++) {
    await client.set({
      type: 'team',
      name: 'team region ' + i,
      parents: {
        $add: i < 6 ? regions[0] : regions[1]
      }
    })
  }

  const dutchteams = await client.get({
    name: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $field: 'ancestors',
            $operator: '=',
            $value: regions[0]
          },
          {
            $field: 'type',
            $operator: '=',
            $value: 'team'
          }
        ]
      }
    }
  })

  t.deepEqualIgnoreOrder(
    dutchteams,
    [
      { name: 'team region 5' },
      { name: 'team region 4' },
      { name: 'team region 3' },
      { name: 'team region 2' },
      { name: 'team region 1' },
      { name: 'team region 0' }
    ],
    'dutch teams'
  )
})
