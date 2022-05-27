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
  await wait(100)
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
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

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - nodeId of a compressed subtree head will restore the compressed subtree', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    description: { en: 'compress me well' },
    children: [
      {
        $id: 'ma2',
        title: { en: 'hello' },
        value: 11,
        description: { en: 'compress me well' },
      },
      {
        $id: 'ma3',
        title: { en: 'hello' },
        value: 12,
        description: { en: 'compress me well' },
      }
    ],
  })

  t.deepEqual(await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'ma1'), 1)
  t.deepEqualIgnoreOrder(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), ['ma1', 'ma2', 'ma3'])

  t.deepEqual(
    await client.get({
      $id: 'ma1',
      id: true,
      title: true,
      value: true,
      description: true,
      d: {
        $list: {
          $find: {
            $traverse: 'descendants',
          }
        },
        id: true,
        title: true,
        value: true,
        description: true,
      }
    }),
    {
      id: 'ma1',
      title: { de: 'hallo' },
      value: 10,
      description: { en: 'compress me well' },
      d: [
        { id: 'ma2', title: { en: 'hello' }, value: 11, description: { en: 'compress me well' } },
        { id: 'ma3', title: { en: 'hello' }, value: 12, description: { en: 'compress me well' } }
      ]
    }
  )

  t.deepEqual(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), [])

  await client.destroy()
})

test.serial('Get with a nodeId of a compressed node will restore the whole subtree', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    description: { en: 'compress me well' },
    children: [
      {
        $id: 'ma2',
        title: { en: 'hello' },
        value: 11,
        description: { en: 'compress me well' },
      },
      {
        $id: 'ma3',
        title: { en: 'hello' },
        value: 12,
        description: { en: 'compress me well' },
      }
    ],
  })

  t.deepEqual(await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'ma1'), 1)
  t.deepEqualIgnoreOrder(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), ['ma1', 'ma2', 'ma3'])

  t.deepEqual(
    await client.get({
      $id: 'ma2',
      id: true,
      title: true,
      value: true,
      description: true,
    }),
    {
      id: 'ma2',
      title: { en: 'hello' },
      value: 11,
      description: { en: 'compress me well' },
    }
  )

  t.deepEqual(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), [])

  t.deepEqual(
    await client.get({
      $id: 'ma1',
      id: true,
      title: true,
      value: true,
      description: true,
      d: {
        $list: {
          $find: {
            $traverse: 'descendants',
          }
        },
        id: true,
        title: true,
        value: true,
        description: true,
      }
    }),
    {
      id: 'ma1',
      title: { de: 'hallo' },
      value: 10,
      description: { en: 'compress me well' },
      d: [
        { id: 'ma2', title: { en: 'hello' }, value: 11, description: { en: 'compress me well' } },
        { id: 'ma3', title: { en: 'hello' }, value: 12, description: { en: 'compress me well' } }
      ]
    }
  )

  await client.destroy()
})

test.serial('find - traversing root will restore compressed subtree', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    description: { en: 'compress me well' },
    children: [
      {
        $id: 'ma2',
        title: { en: 'hello' },
        value: 11,
        description: { en: 'compress me well' },
      },
      {
        $id: 'ma3',
        title: { en: 'hello' },
        value: 12,
        description: { en: 'compress me well' },
      }
    ],
  })

  t.deepEqual(await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'ma1'), 1)
  t.deepEqualIgnoreOrder(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), ['ma1', 'ma2', 'ma3'])

  t.deepEqual(
    await client.get({
      $id: 'root',
      d: {
        $list: {
          $find: {
            $traverse: 'descendants',
          }
        },
        id: true,
        title: true,
        value: true,
        description: true,
      }
    }),
    {
      d: [
        {
          description: {
            en: 'compress me well',
          },
          id: 'ma1',
          title: {
            de: 'hallo',
          },
          value: 10,
        },
        {
          description: {
            en: 'compress me well',
          },
          id: 'ma2',
          title: {
            en: 'hello',
          },
          value: 11,
        },
        {
          description: {
            en: 'compress me well',
          },
          id: 'ma3',
          title: {
            en: 'hello',
          },
          value: 12,
        },
      ],
    }
  )

  t.deepEqual(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), [])

  await client.destroy()
})

test.serial('loop in a subtree', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    description: { en: 'compress me well' },
    children: [
      {
        $id: 'ma2',
        title: { en: 'hello' },
        value: 11,
        description: { en: 'compress me well' },
        children: [
          {
            $id: 'ma3',
            title: { en: 'last' },
          }
        ]
      },
    ],
  })
  await client.set({
    $id: 'ma3',
    children: [ 'ma1' ]
  })

  t.deepEqual(await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'ma1'), 1)

  await client.destroy()
})

test.serial('not a proper subtree', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    description: { en: 'compress me well' },
    children: [
      {
        $id: 'ma2',
        title: { en: 'hello' },
        value: 11,
        description: { en: 'compress me well' },
        children: [
          {
            $id: 'ma3',
            title: { en: 'last' },
          }
        ]
      },
    ],
  })
  await client.set({
    $id: 'ma4',
    children: { $add: [ 'ma3' ] },
  })

  await t.throwsAsync(() => client.redis.selva_hierarchy_compress('___selva_hierarchy', 'ma1'))

  await client.destroy()
})

test.serial('Compress to disk', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    description: { en: 'compress me well' },
    children: [
      {
        $id: 'ma2',
        title: { en: 'hello' },
        value: 11,
        description: { en: 'compress me well' },
      },
      {
        $id: 'ma3',
        title: { en: 'hello' },
        value: 12,
        description: { en: 'compress me well' },
      }
    ],
  })

  t.deepEqual(await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'ma1', 'disk'), 1)
  t.deepEqualIgnoreOrder(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), ['ma1', 'ma2', 'ma3'])

  // TODO Test that we didn't fallback to inmem

  await client.delete('root')
  t.deepEqualIgnoreOrder(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), [])

  await client.destroy()
})

test.serial('Restore from disk', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    description: { en: 'compress me well' },
    children: [
      {
        $id: 'ma2',
        title: { en: 'hello' },
        value: 11,
        description: { en: 'compress me well' },
      },
      {
        $id: 'ma3',
        title: { en: 'hello' },
        value: 12,
        description: { en: 'compress me well' },
      }
    ],
  })

  t.deepEqual(await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'ma1', 'disk'), 1)
  t.deepEqualIgnoreOrder(await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy'), ['ma1', 'ma2', 'ma3'])

  // TODO Test that we didn't fallback to inmem

  t.deepEqual(
    await client.get({ $id: 'ma1', $all: true }),
    {
      description: {
        en: 'compress me well',
      },
      id: 'ma1',
      title: {
        de: 'hallo',
      },
      type: 'match',
      value: 10,
    }
  )

  await client.destroy()
})
