import test from 'ava'
import { performance } from 'perf_hooks'
import { connect } from '../../src/index'
import { start } from '@saulx/selva-server'
import '../assertions'
import { wait } from '../assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
        },
      },
      club: {
        prefix: 'cl',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
        },
      },
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          flupriflu: { type: 'string' },
          date: { type: 'number' },
          value: { type: 'number' },
          status: { type: 'number' },
        },
      },
      video: {
        prefix: 'vi',
        fields: {
          title: { type: 'text' },
          date: { type: 'number' },
          value: { type: 'number' },
        },
      },
    },
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
          parents: { $add: team1 },
        })
      } else {
        ch.push({
          type: 'match',
          name: 'match' + i,
          status: 100,
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
        title: { en: 'flap' },
        date: Date.now() + i + (i > 5 ? 1000000 : -100000),
        value: i,
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
          children: {
            $add: genVideos(),
          },
        },
      ],
    }),
    client.set({
      type: 'league',
      name: 'league 1',
      // @ts-ignore
      children: genMatches(),
    }),
    client.set({
      type: 'league',
      name: 'league 2',
      // @ts-ignore
      children: genMatches(amount),
    }),
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

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial.failing('perf: find compression perf - descendants', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await wait(2e3)

  const leagues = (await client.get({
    n: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'league',
          }
        }
      }
    }
  })).n.map(({ id }) => id)

  const start = performance.now()
  for (let i = 0; i < 1000; i++) {
    // Compress all leagues
    //const startCompress = performance.now()
    await Promise.all(leagues.map(async (id) => {
      try {
        // TODO Sometimes we create leagues that cannot be compresed!
        await client.redis.selva_hierarchy_compress('___selva_hierarchy', id)
      } catch (e) { }
    }))
    //const endCompress = performance.now()
    //console.log('Compressing leagues took:', endCompress - startCompress);

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
          $limit: 1000,
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
                  $value: [300, 2],
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
                          $value: 'now',
                        },
                      },
                    },
                  },
                },
              },
              {
                $operator: '!=',
                $field: 'name',
                $value: ['match1', 'match2', 'match3'],
              },
            ],
          },
        },
      },
    })
  }

  const time = performance.now() - start
  console.log(time, 'ms')
})
