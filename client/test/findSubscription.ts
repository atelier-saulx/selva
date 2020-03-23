import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

import { performance, PerformanceObserver } from 'perf_hooks'

let totalTime = 0
let t
let cnt = 0
const obs = new PerformanceObserver(items => {
  totalTime += items.getEntries()[0].duration
  performance.clearMarks()
  cnt++
  clearTimeout(t)
  t = setTimeout(() => {
    console.log(
      'SPEND',
      totalTime,
      'ms in publish parser',
      'called ',
      cnt,
      'times',
      'avg',
      totalTime / cnt,
      'ms'
    )
    cnt = 0
    totalTime = 0
  }, 500)
})
obs.observe({ entryTypes: ['measure'] })

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } }
    },
    types: {
      league: {
        prefix: 'le',
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
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('subscription find', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const matches = []
  const teams = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team'
    })
  }

  for (let i = 0; i < 10; i++) {
    matches.push({
      $id: await client.id({ type: 'match' }),
      name: 'match ' + i,
      type: 'match',
      value: i,
      parents: {
        $add: [
          teams[~~(Math.random() * teams.length)].$id,
          teams[~~(Math.random() * teams.length)].$id
        ]
      },
      status: i < 5 ? 100 : 300
    })
  }

  await Promise.all(teams.map(t => client.set(t)))

  const league = await client.set({
    type: 'league',
    name: 'league 1',
    children: matches
  })

  await wait(100)
  const obs = await client.observe({
    items: {
      name: true,
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match'
            },
            {
              $field: 'value',
              $operator: '..',
              $value: [5, 10]
            }
          ]
        }
      }
    }
  })
  let cnt = 0
  const sub = obs.subscribe(d => {
    cnt++
    console.log('furpy!', cnt, d)
  })

  await wait(300)
  t.is(cnt, 1)

  await client.set({
    $id: matches[0].$id,
    value: 8
  })

  await wait(1000)
  t.is(cnt, 2)

  await client.set({
    $id: matches[1].$id,
    value: 8
  })
  await wait(300)
  t.is(cnt, 3)

  sub.unsubscribe()

  const obs2 = await client.observe({
    $includeMeta: true,
    items: {
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match'
            }
          ]
        }
      },
      name: true,
      id: true,
      teams: {
        id: true,
        name: true,
        $list: {
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'team'
              }
            ]
          }
        }
      }
    }
  })

  let cnt2 = 0
  const sub2 = obs2.subscribe(d => {
    cnt2++
  })

  await wait(300)
  t.is(cnt2, 1)

  let matchTeam
  for (let i = 0; i < 10; i++) {
    matches.forEach(m => {
      m.value = 8
      m.parents = {
        $add: [
          (matchTeam = teams[~~(Math.random() * teams.length)].$id),
          teams[~~(Math.random() * teams.length)].$id
        ]
      }
    })
  }

  await Promise.all(matches.map(t => client.set(t)))

  await wait(300)
  t.is(cnt2, 2)

  sub2.unsubscribe()

  const obs3 = await client.observe({
    $id: matchTeam,
    $includeMeta: true,
    children: {
      name: true,
      $list: {
        $find: {
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match'
            },
            {
              $field: 'value',
              $operator: '..',
              $value: [5, 10]
            }
          ]
        }
      }
    }
  })

  let cnt3 = 0
  obs3.subscribe(s => {
    cnt3++
  })

  await wait(300)
  // how to handle large responses ???

  // remove unpack
  const amount = 5000 // 10k wrong 5k fine

  const x = []
  for (let i = 0; i < amount; i++) {
    x.push(
      client.set({
        type: 'match',
        value: i,
        parents: { $add: matchTeam }
      })
    )
  }

  var d = Date.now()
  const ids = await Promise.all(x)
  console.log('SET 5k', Date.now() - d, 'ms')

  await wait(2000)
  client.set({
    $id: ids[6],
    name: 'FLURRRRP'
  })
  await wait(1000)

  t.is(cnt3, 3)
})
