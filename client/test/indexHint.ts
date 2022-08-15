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
    selvaOptions: ['FIND_INDICES_MAX', '100', 'FIND_INDEXING_INTERVAL', '1000', 'FIND_INDEXING_ICB_UPDATE_INTERVAL', '500', 'FIND_INDEXING_POPULARITY_AVE_PERIOD', '3', 'FIND_INDEXING_THRESHOLD', '0'],
  })

  await wait(100);
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
      },
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          strValue: { type: 'string' },
          description: { type: 'text' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await wait(100);

  await client.destroy()
})

test.afterEach(async (t) => {
  const client = connect({ port: port })
  await new Promise((r) => setTimeout(r, 100))
  await client.delete('root')
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('create and destroy an index', async (t) => {
  const client = connect({ port })
  const q = ['', '___selva_hierarchy', 'descendants', 'index', '"value" g #20 I', 'fields', 'strValue', 'root', '"value" g #10 I']
  const expected = [ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' ]

  for (let i = 0; i < 100; i++) {
    await client.set({
      type: 'match',
      title: { en: 'a', de: 'b', nl: 'c' },
      value: i,
      strValue: `${i}`,
    })
  }

  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find(...q)
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), expected)
  }
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find(...q)
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), expected)
  }

  t.deepEqual(
    (await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]),
    [ 'root.J.InZhbHVlIiBnICMyMCBJ', '20' ]
  )

  await client.redis.selva_index_del('___selva_hierarchy', 'root.J.InZhbHVlIiBnICMyMCBJ')
  t.deepEqual(
    (await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]),
    []
  )

  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find(...q)
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), expected)
  }
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find(...q)
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), expected)
  }

  t.deepEqual(
    (await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]),
    [ 'root.J.InZhbHVlIiBnICMyMCBJ', '20' ]
  )

  await client.redis.selva_index_del('___selva_hierarchy', 'root.J.InZhbHVlIiBnICMyMCBJ', 1)
  t.deepEqual(
    (await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]),
    [ 'root.J.InZhbHVlIiBnICMyMCBJ', 'not_active' ]
  )
})

test.serial('add and delete nodes in an index', async (t) => {
  const client = connect({ port })

  const ids = await Promise.all([
    {
      type: 'match',
      title: { en: 'a', de: 'b', nl: 'c' },
      value: 81,
      strValue: '81',
    },
    {
      type: 'match',
      title: { en: 'a', de: 'b', nl: 'c' },
      value: 82,
      strValue: '82',
    },
    {
      type: 'match',
      title: { en: 'a', de: 'b', nl: 'c' },
      value: 93,
      strValue: '93',
    }
  ].map((v) => client.set(v)))

  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'strValue', 'root', '"value" g #90 I')
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), [
      '81',
      '82',
    ])
  }
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'strValue', 'root', '"value" g #90 I')
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), [
      '81',
      '82',
    ])
  }

  await client.set({
    type: 'match',
    value: 84,
    strValue: '84',
  })

  const r1 = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'strValue', 'root', '"value" g #90 I')
  t.deepEqualIgnoreOrder(r1.map((v) => v[1][1]), [
    '81',
    '82',
    '84',
  ])

  await client.delete({ $id: ids[1] })

  const r2 = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'strValue', 'root', '"value" g #90 I')
  t.deepEqualIgnoreOrder(r2.map((v) => v[1][1]), [
    '81',
    '84',
  ])
})
