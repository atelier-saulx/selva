import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import redis, { RedisClient } from '@saulx/redis-client'
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
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' },
          },
        },
      },
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
      {
        $id: 'maTest0002',
        title: { en: 'ma2' },
      },
    ],
  })

  await client.destroy()
})

test.after(async (_t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('get a single keyval', async (t) => {
  const client = connect({ port })

  t.deepEqual(
    await client.redis.selva_object_get('', 'maTest0001', 'title.en'),
    'ma1'
  )
})

test.serial('get all', async (t) => {
  const client = connect({ port })

  t.deepEqual(await client.redis.selva_object_get('', 'maTest0001'), [
    'id',
    'maTest0001',
    'title',
    ['en', 'ma1'],
    'type',
    'match',
  ])
})

test.serial('obj len', async (t) => {
  const client = connect({ port })

  t.deepEqual(await client.redis.selva_object_len('maTest0001'), 3)
})

test.serial('string len', async (t) => {
  const client = connect({ port })

  t.deepEqual(await client.redis.selva_object_len('maTest0001', 'title.en'), 3)
})

test.serial('meta', async (t) => {
  const client = connect({ port })

  await client.redis.selva_object_set('maTest0001', 'a', 's', 'abc')
  t.deepEqual(await client.redis.selva_object_getmeta('maTest0001', 'a'), 0)
  t.deepEqual(
    await client.redis.selva_object_setmeta(
      'maTest0001',
      'a',
      Buffer.from(Uint32Array.from([0xbaddcafe]).buffer)
    ),
    1
  )
  t.deepEqual(await client.redis.selva_object_getmeta('maTest0001', 'a'), 0xbaddcafe)
})

test.serial('deleting deep objects', async (t) => {
  const client = connect({ port })

  await client.redis.selva_object_set('maTest0001', 'a.r.s', 's', 'Hello')
  await client.redis.selva_object_set('maTest0001', 'a.s.s', 's', 'Hallo')
  t.deepEqual(await client.redis.selva_object_len('maTest0001', 'a'), 2)

  await client.redis.selva_object_del('maTest0001', 'a.r')
  t.deepEqual(await client.redis.selva_object_len('maTest0001', 'a'), 1)
})
