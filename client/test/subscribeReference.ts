import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

const langs = ['en', 'a', 'b', 'c', 'd', 'e', 'f']

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: langs,
    types: {
      sport: {
        prefix: 'sp',
        fields: {
          title: { type: 'text' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          venue: { type: 'reference' },
        },
      },
      venue: {
        prefix: 've',
        fields: {
          title: { type: 'text' },
          description: { type: 'text' },
          seats: { type: 'references' },
        },
      },
      seat: {
        prefix: 'st',
        fields: {
          color: { type: 'text' },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscription to a reference', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const menuItem = await client.set({
    $id: 'ma1',
    $language: 'en',
    type: 'match',
    title: 'menu item',
  })
  const sport = await client.set({
    $id: 'sp1',
    $language: 'en',
    type: 'sport',
    title: 'football',
  })
  const seat1 = await client.set({
    $id: 'se1',
    $language: 'en',
    type: 'seat',
    color: 'white',
  })
  const seat2 = await client.set({
    $id: 'se2',
    $language: 'en',
    type: 'seat',
    color: 'red',
  })
  const venue = await client.set({
    $id: 've1',
    $language: 'en',
    type: 'venue',
    title: 'Ipurua Stadium',
    seats: [seat1],
  })
  const venue2 = await client.set({
    $id: 've2',
    $language: 'en',
    type: 'venue',
    title: 'Fake Ipurua Stadium',
    seats: [],
  })
  const match = await client.set({
    $id: 'ma2',
    $language: 'en',
    type: 'match',
    title: 'football match',
    parents: [sport],
  })

  const obs = client.observe({
    $id: match,
    $language: 'en',
    title: true,
    venue: {
      title: true,
      seats: true,
    },
  })
  let n = 0
  const sub = obs.subscribe((v) => {
    console.log('got', v)
    switch (n++) {
      case 0:
        t.deepEqualIgnoreOrder(v, { title: 'football match' })
        break
      case 1:
        t.deepEqualIgnoreOrder(v, {
          title: 'football match',
          venue: { title: 'Ipurua Stadium', seats: [seat1] },
        })
        break
      case 2:
        t.deepEqualIgnoreOrder(v, {
          title: 'football match',
          venue: { title: 'Ipurua Stadium', seats: [seat1, seat2] },
        })
        break
      case 3:
        t.deepEqual(v, {
          title: 'football match',
          venue: {
            seats: [],
            title: 'Fake Ipurua Stadium'
          },
        })
        break
      default:
        t.fail()
    }
  })
  await wait(1e3)
  const [sid] = await client.redis.selva_subscriptions_list(
    '___selva_hierarchy'
  )

  t.deepEqual(
    await client.redis.selva_subscriptions_debug('___selva_hierarchy', sid),
    [
      [
        'sub_id: 6e32369d0504c49bca32c9818804dc10b3bdd7b59e50e387164f178e1ac770a6',
        'marker_id: 1217403185',
        'flags: 0x0004',
        'node_id: "ma2"',
        'dir: edge_field',
        'filter_expression: unset',
        'fields: "title\nseats"',
      ],
      [
        'sub_id: 6e32369d0504c49bca32c9818804dc10b3bdd7b59e50e387164f178e1ac770a6',
        'marker_id: 961614000',
        'flags: 0x0004',
        'node_id: "ma2"',
        'dir: node',
        'filter_expression: unset',
        'fields: "title\nvenue"',
      ],
    ]
  )

  await client.set({
    $id: match,
    venue: venue,
  })
  await wait(1e3)
  await client.set({
    $id: venue,
    seats: { $add: [seat2] },
  })

  t.deepEqual(
    await client.redis.selva_subscriptions_debug('___selva_hierarchy', sid),
    [
      [
        'sub_id: 6e32369d0504c49bca32c9818804dc10b3bdd7b59e50e387164f178e1ac770a6',
        'marker_id: 1217403185',
        'flags: 0x0004',
        'node_id: "ma2"',
        'dir: edge_field',
        'filter_expression: unset',
        'fields: "title\nseats"',
      ],
      [
        'sub_id: 6e32369d0504c49bca32c9818804dc10b3bdd7b59e50e387164f178e1ac770a6',
        'marker_id: 961614000',
        'flags: 0x0004',
        'node_id: "ma2"',
        'dir: node',
        'filter_expression: unset',
        'fields: "title\nvenue"',
      ],
      [
        'sub_id: 6e32369d0504c49bca32c9818804dc10b3bdd7b59e50e387164f178e1ac770a6',
        'marker_id: 983306425',
        'flags: 0x0004',
        'node_id: "ve1"',
        'dir: node',
        'filter_expression: unset',
        'fields: "title\nseats"',
      ],
    ]
  )
  await wait(1e3)
  await client.set({
    $id: match,
    venue: venue2,
  })
  await wait(1e3)
  t.deepEqual(n, 4, 'All change events received')

  t.deepEqual(
    await client.redis.selva_subscriptions_debug('___selva_hierarchy', sid),
    [
      [
        'sub_id: 6e32369d0504c49bca32c9818804dc10b3bdd7b59e50e387164f178e1ac770a6',
        'marker_id: 1217403185',
        'flags: 0x0004',
        'node_id: "ma2"',
        'dir: edge_field',
        'filter_expression: unset',
        'fields: "title\nseats"',
      ],
      [
        'sub_id: 6e32369d0504c49bca32c9818804dc10b3bdd7b59e50e387164f178e1ac770a6',
        'marker_id: 961614000',
        'flags: 0x0004',
        'node_id: "ma2"',
        'dir: node',
        'filter_expression: unset',
        'fields: "title\nvenue"',
      ],
      [
        'sub_id: 6e32369d0504c49bca32c9818804dc10b3bdd7b59e50e387164f178e1ac770a6',
        'marker_id: 985272506',
        'flags: 0x0004',
        'node_id: "ve2"',
        'dir: node',
        'filter_expression: unset',
        'fields: "title\nseats"',
      ],
    ]
  )

  sub.unsubscribe()

  await client.destroy()
})

test.serial('subscription to inherit an edge', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const sport = await client.set({
    $language: 'en',
    type: 'sport',
    title: 'football',
  })
  const seat1 = await client.set({
    $language: 'en',
    type: 'seat',
    color: 'white',
  })
  const seat2 = await client.set({
    $language: 'en',
    type: 'seat',
    color: 'red',
  })
  const venue = await client.set({
    $language: 'en',
    type: 'venue',
    title: 'Ipurua Stadium',
    seats: [seat1],
  })
  const match = await client.set({
    $language: 'en',
    type: 'match',
    title: 'football match',
    parents: [sport],
  })

  const obs = client.observe({
    $id: match,
    $language: 'en',
    title: true,
    venue: {
      $inherit: true,
      title: true,
      seats: true,
    },
  })
  let n = 0
  const sub = obs.subscribe((v) => {
    console.log('got', v)
    switch (n++) {
      case 0:
        t.deepEqualIgnoreOrder(v, { title: 'football match' })
        break
      case 1:
        t.deepEqualIgnoreOrder(v, {
          title: 'football match',
          venue: { title: 'Ipurua Stadium', seats: [seat1] },
        })
        break
      case 2:
        t.deepEqualIgnoreOrder(v, {
          title: 'football match',
          venue: { title: 'Ipurua Stadium', seats: [seat1, seat2] },
        })
        break
      default:
        t.fail()
    }
  })
  await wait(1e3)

  await client.set({
    $id: match,
    venue: venue,
  })
  await wait(1e3)
  await client.set({
    $id: venue,
    seats: { $add: [seat2] },
  })
  await wait(1e3)
  t.deepEqual(n, 3, 'All change events received')

  sub.unsubscribe()

  await client.destroy()
})

test.serial('multiple references to a single node', async (t) => {
  const MATCHES_COUNT = 100
  const client = connect({ port }, { loglevel: 'info' })
  const sport = await client.set({
    $alias: 'sporty',
    $language: 'en',
    type: 'sport',
    title: 'football',
  })
  const venue = await client.set({
    $alias: 'venuey.venue',
    $language: 'en',
    type: 'venue',
    title: 'Ipurua Stadium',
  })

  for (let i = 0; i < MATCHES_COUNT; i++) {
    for (const lang of langs) {
      await client.set({
        $alias: 'match.' + i,
        $language: lang,
        type: 'match',
        title: `football match ${i}`,
        parents: [sport],
        venue: venue,
      })
    }
  }

  const matchIds = (
    await client.get({
      $id: sport,
      children: true,
    })
  ).children

  let count = 0
  const subs = []
  matchIds.forEach((id, idx) => {
    for (const lang of langs) {
      const obs = client.observe({
        $alias: 'match.' + idx,
        $language: lang,
        title: true,
        venue: { title: true },
      })

      let i = 0
      subs.push(
        obs.subscribe((v) => {
          t.regex(v.title, /^football match /)
          if (i == 0) {
            t.deepEqual(v.venue.title, 'Ipurua Stadium')
          } else if (i >= 1 && i <= 10) {
            t.deepEqual(v.venue.title, 'Some Stadium' + i)
          } else {
            t.fail()
          }
          i++
          count++
        })
      )
    }
  })

  for (let i = 1; i < 10; i++) {
    await wait(5000)
    for (const lang of langs) {
      await client.set({
        $alias: 'venuey.venue',
        $language: lang,
        title: 'Some Stadium' + i,
      })
    }
  }

  await wait(500)

  t.deepEqual(count, MATCHES_COUNT * langs.length * 10)

  subs.map((sub) => sub.unsubscribe())

  await client.destroy()
})
