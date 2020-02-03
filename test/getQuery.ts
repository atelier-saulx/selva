import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import queryParser from '../src/query'
import './assertions'
import { wait } from './assertions'

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
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          // need to warn if you change this!!!
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      },
      video: {
        prefix: 'vi',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          // making it different here should tell you something or at least take it over
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })

  const team1 = await client.id({ type: 'team' })

  const genMatches = (s = 0) => {
    const ch = []
    for (let i = s; i < s + 7500; i++) {
      if (i < 100) {
        ch.push({
          type: 'match',
          name: 'match' + i,
          status: i === 0 ? 2 : i > 10 ? 100 : 300,
          parents: { $add: team1 }
        })
      } else {
        ch.push({
          type: 'match',
          name: 'match' + i,
          status: 100
        })
      }
    }
    return ch
  }

  const genVideos = () => {
    const ch = []
    for (let i = 0; i < 100; i++) {
      ch.push({
        type: 'video',
        name: 'video',
        date: Date.now() + i + (i > 5 ? 1000000 : -100000),
        value: i
      })
    }
    return ch
  }

  const d = Date.now()
  const ids = await Promise.all([
    client.set({
      type: 'club',
      name: 'club 1',
      children: [
        {
          $id: team1,
          name: 'team 1',
          children: genVideos()
        }
      ]
    }),
    client.set({
      type: 'league',
      name: 'league 1',
      // @ts-ignore
      children: genMatches()
    }),
    client.set({
      type: 'league',
      name: 'league 2',
      // @ts-ignore
      children: genMatches(7500)
    })
  ])
  console.log('Set 15k nested', Date.now() - d, 'ms')

  await wait(500)
  t.true(ids[0].slice(0, 2) === 'cl' && ids[1].slice(0, 2) === 'le')

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

test.serial('get - queryParser', async t => {
  // simple nested - single query
  const client = connect({ port: 6088 })
  // extra option in find is index or auto from fields
  const results = await client.query({
    name: true,
    value: true,
    status: true,
    date: true,
    id: true,
    type: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: [
          {
            $operator: '=',
            $field: 'type',
            $value: 'match',
            $and: {
              $operator: '=',
              $field: 'status',
              $value: [300, 2] // handle or
            },
            $or: {
              $operator: '=',
              $field: 'name',
              $value: 'league 1',
              $or: {
                $operator: '>',
                $field: 'value',
                $value: 4,
                $and: {
                  $operator: '>',
                  $field: 'value',
                  $value: 6,
                  $and: {
                    $operator: '<',
                    $field: 'value',
                    $value: 8,
                    $and: {
                      $operator: '>',
                      $field: 'date',
                      $value: 'now'
                    }
                  }
                }
              }
            }
          },
          {
            $operator: '!=',
            $field: 'name',
            $value: ['match1', 'match2', 'match3']
          }
        ]
      }
    }
  })

  const matches = results.filter(v => v.type === 'match')
  const videos = results.filter(v => v.type === 'video')
  const league = results.filter(v => v.type === 'league')

  t.is(matches.length, 8, 'query result matches')
  t.is(videos.length, 3, 'query result videos')
  t.is(league.length, 1, 'query result league')

  const team = await client.query({
    id: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: {
          $field: 'type',
          $operator: '=',
          $value: 'team'
        }
      }
    }
  })

  t.true(/te/.test(team[0].id), 'got id from team')

  const teamMatches = await client.query({
    $id: team[0].id,
    id: true,
    $list: {
      $find: {
        $traverse: 'descendants',
        $filter: {
          $field: 'type',
          $operator: '=',
          $value: 'match'
        }
      }
    }
  })

  t.is(teamMatches.length, 100)

  const teamMatchesRange = await client.query({
    $id: team[0].id,
    id: true,
    $list: {
      $range: [0, 5],
      $find: {
        $traverse: 'descendants',
        $filter: {
          $field: 'type',
          $operator: '=',
          $value: 'match'
        }
      }
    }
  })

  t.is(teamMatchesRange.length, 5)

  const videosSorted = await client.query({
    value: true,
    $list: {
      $sort: { $field: 'value', $order: 'desc' },
      $range: [0, 5],
      $find: {
        $traverse: 'descendants',
        $filter: {
          $field: 'type',
          $operator: '=',
          $value: 'video'
        }
      }
    }
  })

  t.deepEqual(
    videosSorted.map(v => v.value),
    [99, 98, 97, 96, 95]
  )
})
