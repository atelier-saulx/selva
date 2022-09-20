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
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('$fieldsByType simple', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      car: {
        prefix: 'ma',
        fields: {
          name: { type: 'text' },
        },
      },
      engine: {
        prefix: 'ng',
        fields: {
          power: { type: 'number' },
          displacement: { type: 'number' },
        },
      },
      tire: {
        prefix: 'tr',
        fields: {
          position: { type: 'string' },
        },
      },
    },
  })

  const car = await client.set({
    type: 'car',
    name: { en: 'Clown wagon' },
    children: [
      {
        type: 'engine',
        power: 25.0,
        displacement: 569.0,
      },
      {
        type: 'tire',
        $id: 'tr1', // This wouldn't be necessary if we could sort by two fields
        position: 'LF',
      },
      {
        type: 'tire',
        $id: 'tr2',
        position: 'RF',
      },
      {
        type: 'tire',
        $id: 'tr3',
        position: 'LR',
      },
      {
        type: 'tire',
        $id: 'tr4',
        position: 'RR',
      },
    ]
  })

  const res = await client.get({
    $id: car,
    name: true,
    parts: {
      $fieldsByType: {
        engine: { type: true, power: true },
        tire: { type: true, position: true },
      },
      $list: {
        $find: { $traverse: 'children' },
      },
    },
  })
  res.parts.sort((a, b) => a.type === b.type ? a.position.localeCompare(b.position) : a.type.localeCompare(b.type))
  t.deepEqual(res, {
    name: { en: 'Clown wagon' },
    parts: [
      { type: 'engine', power: 25 },
      { type: 'tire', position: 'LF' },
      { type: 'tire', position: 'LR' },
      { type: 'tire', position: 'RF' },
      { type: 'tire', position: 'RR' },
    ]
  })

  t.deepEqual(await client.get({
    $id: car,
    name: true,
    parts: {
      $fieldsByType: {
        engine: { type: true, power: true },
        tire: { type: true, position: true },
      },
      $list: {
        $sort: { $field: 'type', $order: 'asc' },
        $find: { $traverse: 'children' },
      },
    },
  }), {
    name: { en: 'Clown wagon' },
    parts: [
      { type: 'engine', power: 25 },
      { type: 'tire', position: 'LF' },
      { type: 'tire', position: 'RF' },
      { type: 'tire', position: 'LR' },
      { type: 'tire', position: 'RR' },
    ]
  })

  await client.destroy()
})

test.serial('$fieldsByType huge', async (t) => {
  const client = connect({ port })

  const types = [...Array(26).keys()].map((v) => String.fromCharCode(v + 97)).map((v) => ({
    prefix: `a${v}`,
    fields: [...Array(50).keys()].map((i) => [`${v}f${i}`, { type: 'string' }]).reduce((prev, cur: ['string', any]) => ({ ...prev, [cur[0]]: cur[1]}), {}),
  })).reduce((prev, cur) => ({ ...prev, [cur.prefix]: cur }), {})

  await client.updateSchema({
    languages: ['en'],
    types
  })

  await Promise.all(Object.keys(types).map((t) => client.set({
    $id: `${t}1`,
    ...Object.keys(types[t].fields).reduce((prev, cur) => ({ ...prev, [cur]: `hello ${t}` }), {}),
  })))

  const fieldsByType = Object.keys(types).reduce((prev, cur) => ({
    ...prev,
    [cur]: {
      type: true,
      parents: true,
      [`${cur.substring(1)}f1`]: true,
    }
  }), {})
  await t.notThrowsAsync(async () => client.get({
    $id: 'root',
    items: {
      $fieldsByType: fieldsByType,
      $list: {
        $offset: 0,
        $limit: 15,
        $sort: {
          $field: 'type',
          $order: 'desc'
        },
        $find: {
          $traverse: 'descendants'
        }
      }
    }
  }))

  await client.destroy()
})
