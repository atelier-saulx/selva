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

test.serial('inherit references $list', async (t) => {
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
    seats: [seat1, seat2],
  })

  const child = await client.set({
    $language: 'en',
    type: 'match',
    title: 'football match',
    parents: [sport],
    venue: venue,
  })

  t.deepEqual(
    await client.get({
      $id: child,
      $language: 'en',
      title: true,
      venue: true,
    }),
    { title: 'football match', venue }
  )

  t.deepEqual(
    await client.get({
      $id: child,
      $language: 'en',
      title: true,
      venue: {
        $inherit: true,
        title: true,
        description: { $default: 'no description' },
      },
    }),
    {
      title: 'football match',
      venue: {
        title: 'Ipurua Stadium',
        description: 'no description',
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: child,
      $language: 'en',
      title: true,
      venue: {
        $inherit: true,
        no: true,
      },
    }),
    { title: 'football match' },
    'Inherit non-existing field'
  )

  t.deepEqual(
    await client.get({
      $id: child,
      $language: 'en',
      title: true,
      venue: {
        $inherit: true,
        // TODO Id
      },
    }),
    { title: 'football match', venue },
    'Inherit all fields from venue'
  )

  t.deepEqual(
    await client.get({
      $id: child,
      $language: 'en',
      title: true,
      venue: {
        $inherit: true,
        $all: true,
      },
    }),
    {
      title: 'football match',
      venue: { id: venue, type: 'venue', title: 'Ipurua Stadium' },
    },
    'Inherit all fields from venue'
  )

  // inheriting non-single ref edge field should fail
  t.deepEqualIgnoreOrder(
    await client.get({
      $id: venue,
      $language: 'en',
      title: true,
      seats: {
        $inherit: true,
      },
    }),
    { title: 'Ipurua Stadium', seats: [seat1, seat2] }
  )

  await client.destroy()
})
