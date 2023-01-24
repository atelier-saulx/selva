import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
let srv2
let port2: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  port2 = await getPort()
  srv2 = await startOrigin({
    name: 'matches',
    registry: { port },
    port: port2,
  })

  await wait(1000)

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } },
    },
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string' },
        },
      },
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string' },
        },
      },
    },
  })

  await client.updateSchema(
    {
      languages: ['en'],
      rootType: {
        fields: { yesh: { type: 'string' }, no: { type: 'string' } },
      },
      types: {
        match: {
          prefix: 'ma',
          fields: {
            name: { type: 'string' },
            value: {
              type: 'number',
            },
            status: {
              type: 'number',
            },
            date: { type: 'number' },
          },
        },
      },
    },
    'matches'
  )
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.delete({ $id: 'root', $db: 'matches' })
  await client.destroy()
  await srv.destroy()
  await srv2.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscription find multi-db', async (t) => {
  const client = connect({ port })

  const matches: any[] = []
  const teams: any[] = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team',
    })
  }

  for (let i = 0; i < 10; i++) {
    matches.push({
      $db: 'matches',
      $id: await client.id({ db: 'matches', type: 'match' }),
      name: 'match ' + i,
      type: 'match',
      value: i,
      // parents: {
      //   $add: [
      //     teams[~~(Math.random() * teams.length)].$id,
      //     teams[~~(Math.random() * teams.length)].$id,
      //   ],
      // },
      status: i < 5 ? 100 : 300,
    })
  }

  await Promise.all(teams.map((t) => client.set(t)))
  const matchIds = await Promise.all(matches.map((t) => client.set(t)))

  const league = await client.set({
    type: 'league',
    name: 'league 1',
    children: matchIds,
  })

  // console.log(
  //   JSON.stringify(
  //     await client.get({
  //       $id: league,
  //       items: {
  //         $id: 'root',
  //         $db: 'matches',
  //         children: {
  //           id: true,
  //           name: true,
  //           $list: {
  //             $sort: { $field: 'name', $order: 'asc' },
  //             $limit: 100,
  //             $offset: 0,
  //           },
  //         },
  //       },
  //     }),
  //     null,
  //     2
  //   )
  // )
  // return

  await wait(100)
  // TODO: make this work
  const obs = client.observe({
    $id: league,
    items: {
      $id: 'root',
      $db: 'matches',
      children: {
        id: true,
        name: true,
        value: true,
        $list: {
          $sort: { $field: 'value', $order: 'asc' },
          $limit: 100,
          $offset: 0,
        },
      },
    },
  })

  // const obs = client.observe({
  //   $id: league,
  //   children: {
  //     $db: 'matches',
  //     id: true,
  //     name: true,
  //     value: true,
  //     $list: {
  //       $sort: { $field: 'value', $order: 'asc' },
  //       $limit: 100,
  //       $offset: 0,
  //     },
  //   },
  // })

  // already works
  // const obs = client.observe({
  //   $id: 'root',
  //   $db: 'matches',
  //   children: {
  //     id: true,
  //     name: true,
  //     value: true,
  //     $list: {
  //       $sort: { $field: 'value', $order: 'asc' },
  //       $limit: 100,
  //       $offset: 0,
  //     },
  //   },
  // })

  let cnt = 0
  const sub = obs.subscribe((d) => {
    cnt++
  })

  await wait(1000)
  // for (const db of ['default', 'matches']) {
  //   const subs = await client.redis.selva_subscriptions_list(
  //     { name: db },
  //     '___selva_hierarchy'
  //   )
  //   const debug = await Promise.all(
  //     subs.map((s) =>
  //       client.redis.selva_subscriptions_debug(
  //         { name: db },
  //         '___selva_hierarchy',
  //         s
  //       )
  //     )
  //   )
  //   console.log('HMM', subs, debug)
  // }
  t.is(cnt, 1)

  await client.set({
    $db: 'matches',
    $id: matches[0].$id,
    value: 8,
  })

  await wait(1000)
  t.is(cnt, 2)

  await client.set({
    $db: 'matches',
    $id: matches[1].$id,
    value: 8,
  })
  await wait(1000)
  t.is(cnt, 3)

  sub.unsubscribe()
  await client.destroy()
})
