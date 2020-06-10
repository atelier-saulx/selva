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

  await wait(500)
  // { loglevel: 'info' }
  const client = connect({ port })
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
          flupriflu: { type: 'string' },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          // need to warn if you change this!!!
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      },
      video: {
        prefix: 'vi',
        fields: {
          title: { type: 'text', search: { type: ['TEXT-LANGUAGE-SUG'] } },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          // making it different here should tell you something or at least take it over
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })

  const team1 = await client.id({ type: 'team' })
  const amount = 50000
  const vids = 100
  const genMatches = (s = 0) => {
    const ch = []
    for (let i = s; i < s + amount; i++) {
      if (i < 1000) {
        ch.push({
          type: 'match',
          flupriflu: 'true',
          name: 'match' + i,
          status: i === 0 ? 2 : i > 1000 ? 100 : 300,
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
    for (let i = 0; i < vids; i++) {
      ch.push({
        type: 'video',
        name: 'video',
        title: { en: 'flap video ' + i },
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
      children: genMatches(amount)
    })
  ])
  console.log(
    `Set ${Math.floor((amount * 2 + vids) / 100) / 10}k nested`,
    Date.now() - d,
    'ms'
  )

  await wait(600)
  t.true(ids[0].slice(0, 2) === 'cl' && ids[1].slice(0, 2) === 'le')

  // const matches = (await dumpDb(client)).filter(v => {
  //   if (typeof v[1] === 'object' && v[1].type === 'match') {
  //     return true
  //   }
  //   return false
  // })

  // console.log(matches, matches.length)

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

test.serial('find - descendants', async t => {
  // simple nested - single query

  try {
    const client = connect({ port }, { loglevel: 'info' })

    // extra option in find is index or auto from fields
    let d = Date.now()
    const { items: results } = await client.get({
      items: {
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
            $filter: [
              {
                $operator: '=',
                $field: 'type',
                $value: 'match',
                $and: {
                  $operator: '=',
                  $field: 'status',
                  $value: [300, 2]
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
      }
    })

    console.log('Executing query (1100 resuls)', Date.now() - d, 'ms')

    const matches = results.filter(v => v.type === 'match')
    const videos = results.filter(v => v.type === 'video')
    const league = results.filter(v => v.type === 'league')

    t.is(matches.length, 997, 'query result matches')
    t.is(videos.length, 3, 'query result videos')
    t.is(league.length, 1, 'query result league')

    const team = (
      await client.get({
        items: {
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
        }
      })
    ).items

    t.true(/te/.test(team[0].id), 'got id from team')

    const { items: teamMatches } = await client.get({
      $id: team[0].id,
      items: {
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
      }
    })

    t.is(teamMatches.length, 1000)

    const { items: teamMatchesRange } = await client.get({
      $id: team[0].id,
      items: {
        id: true,
        $list: {
          $limit: 5,
          $find: {
            $traverse: 'descendants',
            $filter: {
              $field: 'type',
              $operator: '=',
              $value: 'match'
            }
          }
        }
      }
    })

    t.is(teamMatchesRange.length, 5)

    const { items: videosSorted } = await client.get({
      items: {
        value: true,
        $list: {
          $sort: { $field: 'value', $order: 'desc' },
          $limit: 5,
          $find: {
            $traverse: 'descendants',
            $filter: {
              $field: 'type',
              $operator: '=',
              $value: 'video'
            }
          }
        }
      }
    })

    t.deepEqual(
      videosSorted.map(v => v.value),
      [99, 98, 97, 96, 95]
    )

    const { items: empty } = await client.get({
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $field: 'name',
                $value: 'gurk'
              },
              {
                $operator: '=',
                $field: 'name',
                $value: ['flap', 'gurk']
              }
            ]
          }
        }
      }
    })

    await wait(1000)

    //@ts-ignore
    t.deepEqual(empty, [], 'does not throw for TAG fields')

    await wait(1000)

    const { items: videosText } = await client.get({
      items: {
        value: true,
        $list: {
          $sort: { $field: 'value', $order: 'desc' },
          $limit: 5,
          $find: {
            $traverse: 'descendants',
            $filter: {
              $field: 'title',
              $operator: '=',
              $value: 'flap'
            }
          }
        }
      }
    })

    t.deepEqual(videosText, [
      { value: 99 },
      { value: 98 },
      { value: 97 },
      { value: 96 },
      { value: 95 }
    ])
  } catch (err) {
    console.error(err)
  }
})
