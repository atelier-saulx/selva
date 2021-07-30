import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

const toCArr = (arr) => arr.map(s => s.padEnd(10, '\0')).join('')

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(100);
  console.log('lollers')
  //await wait(60e3);
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
          description: { type: 'text' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await wait(100);

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('create and destroy an index', async (t) => {
  const client = connect({ port })

  for (let i = 0; i < 100; i++) {
    await client.set({
      type: 'match',
      title: { en: 'a', de: 'b', nl: 'c' },
      value: i,
    })
  }

  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'value', 'root', '"value" g #90 H')
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), [
      '93',
      '95',
      '91',
      '96',
      '94',
      '98',
      '92',
      '99',
      '97'
    ])
  }
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'value', 'root', '"value" g #90 H')
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), [
      '93',
      '95',
      '91',
      '96',
      '94',
      '98',
      '92',
      '99',
      '97'
    ])
  }

  t.deepEqual(
    await client.redis.selva_index_list('___selva_hierarchy'),
    [ 'root.I.InZhbHVlIiBnICM4MCBI', 19 ]
  )

  await client.redis.selva_index_del('___selva_hierarchy', 'root.I.InZhbHVlIiBnICM4MCBI')
  t.deepEqual(
    await client.redis.selva_index_list('___selva_hierarchy'),
    []
  )

  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'value', 'root', '"value" g #90 H')
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), [
      '93',
      '95',
      '91',
      '96',
      '94',
      '98',
      '92',
      '99',
      '97'
    ])
  }
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'value', 'root', '"value" g #90 H')
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), [
      '93',
      '95',
      '91',
      '96',
      '94',
      '98',
      '92',
      '99',
      '97'
    ])
  }

  t.deepEqual(
    await client.redis.selva_index_list('___selva_hierarchy'),
    [ 'root.I.InZhbHVlIiBnICM4MCBI', 19 ]
  )

  await client.redis.selva_index_del('___selva_hierarchy', 'root.I.InZhbHVlIiBnICM4MCBI', 1)
  t.deepEqual(
    await client.redis.selva_index_list('___selva_hierarchy'),
    [ 'root.I.InZhbHVlIiBnICM4MCBI', 'not_active' ]
  )
})

test.serial.only('add and delete nodes in an index', async (t) => {
  const client = connect({ port })

  const ids = await Promise.all([
    {
      type: 'match',
      title: { en: 'a', de: 'b', nl: 'c' },
      value: 81,
    },
    {
      type: 'match',
      title: { en: 'a', de: 'b', nl: 'c' },
      value: 82,
    },
    {
      type: 'match',
      title: { en: 'a', de: 'b', nl: 'c' },
      value: 93,
    }
  ].map((v) => client.set(v)))

  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'value', 'root', '"value" g #90 I')
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), [
      '81',
      '82',
    ])
  }
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'value', 'root', '"value" g #90 I')
    t.deepEqualIgnoreOrder(r.map((v) => v[1][1]), [
      '81',
      '82',
    ])
  }

  await client.set({
    type: 'match',
    value: 84,
  })

  const r1 = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'value', 'root', '"value" g #90 I')
  t.deepEqualIgnoreOrder(r1.map((v) => v[1][1]), [
    '81',
    '82',
    '84',
  ])

  await client.delete({ $id: ids[1] })

  const r2 = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', '"value" g #80 H', 'fields', 'value', 'root', '"value" g #90 I')
  t.deepEqualIgnoreOrder(r2.map((v) => v[1][1]), [
    '81',
    '84',
  ])
})
