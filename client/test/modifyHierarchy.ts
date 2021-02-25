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
  await wait(100)

  await client.destroy()
})

test.after(async (_t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('complex hierarchy on one set', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
      {
        $id: 'maTest0011', // child of the first level
        title: { en: 'ma11' },
        children: [
          {
            $id: 'maTest0021',
            title: { en: 'ma21' },
          },
        ],
        parents: [
          {
            $id: 'maTest0002', // Additional parent
            title: { en: 'ma02' },
          },
        ],
      },
      {
        $id: 'maTest0012', // child of the first level
        title: { en: 'ma12' },
      },
      {
        $id: 'maTest0013', // child of the first level
        title: { en: 'ma13' },
        children: [
          {
            $id: 'maTest0021',
            title: { en: 'ma21' },
            children: [
              {
                $id: 'maTest0031',
                title: { en: 'ma31' },
              },
            ],
          },
        ],
      },
    ],
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0001'
    ),
    ['root']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0011'
    ),
    ['maTest0001', 'maTest0002']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0012'
    ),
    ['maTest0001']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0013'
    ),
    ['maTest0001']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0021'
    ),
    ['maTest0011', 'maTest0013']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0001'
    ),
    ['maTest0011', 'maTest0012', 'maTest0013']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0002'
    ),
    ['maTest0011']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0011'
    ),
    ['maTest0021']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0012'
    ),
    []
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0013'
    ),
    ['maTest0021']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0021'
    ),
    ['maTest0031']
  )
})

test.serial('complex hierarchy on two sets', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
      {
        $id: 'maTest0011', // child of the first level
        title: { en: 'ma11' },
        parents: [
          {
            $id: 'maTest0002', // Additional parent
            title: { en: 'ma02' },
          },
        ],
      },
      {
        $id: 'maTest0012', // child of the first level
        title: { en: 'ma12' },
      },
      {
        $id: 'maTest0013', // child of the first level
        title: { en: 'ma13' },
      },
    ],
  })

  await client.set({
    $id: 'maTest0021',
    title: { en: 'ma21' },
    parents: ['maTest0013', 'maTest0011'],
    children: [
      {
        $id: 'maTest0031',
        title: { en: 'ma31' },
      },
    ],
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0001'
    ),
    ['root']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0011'
    ),
    ['maTest0001', 'maTest0002']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0012'
    ),
    ['maTest0001']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0013'
    ),
    ['maTest0001']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0021'
    ),
    ['maTest0011', 'maTest0013']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0001'
    ),
    ['maTest0011', 'maTest0012', 'maTest0013']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0002'
    ),
    ['maTest0011']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0011'
    ),
    ['maTest0021']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0012'
    ),
    []
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0013'
    ),
    ['maTest0021']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0021'
    ),
    ['maTest0031']
  )
})

test.serial('complex hierarchy using add', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
      {
        $id: 'maTest0011', // child of the first level
        title: { en: 'ma11' },
        parents: [
          {
            $id: 'maTest0002', // Additional parent
            title: { en: 'ma02' },
          },
        ],
      },
      {
        $id: 'maTest0012', // child of the first level
        title: { en: 'ma12' },
      },
      {
        $id: 'maTest0013', // child of the first level
        title: { en: 'ma13' },
        children: [
          {
            $id: 'maTest0021',
            title: { en: 'ma21' },
            children: [
              {
                $id: 'maTest0031',
                title: { en: 'ma31' },
              },
            ],
          },
        ],
      },
    ],
  })

  await client.set({
    $id: 'maTest0021',
    parents: {
        $add: [ 'maTest0011' ]
      },
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0001'
    ),
    ['root']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0011'
    ),
    ['maTest0001', 'maTest0002']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0012'
    ),
    ['maTest0001']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0013'
    ),
    ['maTest0001']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0021'
    ),
    ['maTest0011', 'maTest0013']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0001'
    ),
    ['maTest0011', 'maTest0012', 'maTest0013']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0002'
    ),
    ['maTest0011']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0011'
    ),
    ['maTest0021']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0012'
    ),
    []
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0013'
    ),
    ['maTest0021']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0021'
    ),
    ['maTest0031']
  )
})

test.serial('Undo complex hierarchy using set', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
      {
        $id: 'maTest0011', // child of the first level
        title: { en: 'ma11' },
        parents: [
          {
            $id: 'maTest0002', // Additional parent
            title: { en: 'ma02' },
          },
        ],
      },
      {
        $id: 'maTest0012', // child of the first level
        title: { en: 'ma12' },
      },
      {
        $id: 'maTest0013', // child of the first level
        title: { en: 'ma13' },
        children: [
          {
            $id: 'maTest0021',
            title: { en: 'ma21' },
            children: [
              {
                $id: 'maTest0031',
                title: { en: 'ma31' },
              },
            ],
          },
        ],
      },
    ],
  })

  await client.set({
    $id: 'maTest0021',
    parents: {
        $add: [ 'maTest0011' ]
      },
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0021'
    ),
    ['maTest0011', 'maTest0013']
  )

  await client.set({
    $id: 'maTest0021',
    parents: [ 'maTest0013' ],
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_parents(
      '___selva_hierarchy',
      'maTest0021'
    ),
    ['maTest0013']
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0011'
    ),
    []
  )
  t.deepEqualIgnoreOrder(
    await client.redis.selva_hierarchy_children(
      '___selva_hierarchy',
      'maTest0013'
    ),
    ['maTest0021']
  )
})
