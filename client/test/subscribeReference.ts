import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
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

test.serial.only('subscription to a reference', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const menuItem = await client.set({
    $language: 'en',
    type: 'match',
    title: 'menu item',
  })
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
  const venue2 = await client.set({
    $language: 'en',
    type: 'venue',
    title: 'Fake Ipurua Stadium',
    seats: [],
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
          venue: { title: 'Fake Ipurua Stadium' },
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
  console.log(
    await client.redis.selva_subscriptions_debug('___selva_hierarchy', sid)
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
  await wait(1e3)
  await client.set({
    $id: match,
    venue: venue2,
  })
  await wait(1e3)
  t.deepEqual(n, 4, 'All change events received')

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
