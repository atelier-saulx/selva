import test from 'ava'
const { compile, createRecord } = require('data-record')
import { connect } from '../src/index'
import { setRecordDefCstring } from '../src/set/modifyDataRecords'
import { start } from '@saulx/selva-server'
import redis, { RedisClient } from 'redis'
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

  await client.destroy()
})

test.after(async (_t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('basic edge ops', async (t) => {
  const client = connect({ port })

  // Create nodes
  await client.redis.selva_modify('root', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma1', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma2', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma3', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma4', '', '0', 'o.a', 'hello')

  // Create edges
  await client.redis.selva_modify('ma2', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma3']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma3', '', '5', 'refs', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma4']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma4', '', '5', 'refs', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma2']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'ma1'),
    []
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'ma2'),
    ['a', [
      'b', [
       'ma1',
       'ma3',
      ],
    ]]
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_edgelist('___selva_hierarchy', 'ma2', 'a'),
    ['b', [
      'ma1',
       'ma3',
      ],
    ]
  )

  await t.throwsAsync(() => client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma2', 'a'))

  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma2', 'a.b'),
    [ 0, 'ma1', 'ma3' ]
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma3', 'refs'),
    [ 0, 'ma4' ]
  )

  // Delete ma3
  await client.redis.selva_hierarchy_del('___selva_hierarchy', 'ma3'),

  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma2', 'a.b'),
    [ 0, 'ma1' ]
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_edgeget('___selva_hierarchy', 'ma4', 'refs'),
    [ 0, 'ma2' ]
  )
})

test.serial('traverse a custom field', async (t) => {
  const client = connect({ port })

  // Create nodes
  await client.redis.selva_modify('root', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma1', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma2', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma3', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma4', '', '0', 'o.a', 'hello')

  // Create edges
  await client.redis.selva_modify('root', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma1', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma3']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma2', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma4']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'bfs', 'a.b', 'root'),
    [ 'root', 'ma1', 'ma2', 'ma3', 'ma4' ]
  )
  // Currently an error is not returned even though dfs is not supported, instead
  // an invalid traversal option just skips the node in the list.
  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'dfs', 'a.b', 'root'),
    []
  )
})

test.serial('find can return edge fields', async (t) => {
  const client = connect({ port })

  // Create nodes
  await client.redis.selva_modify('root', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma1', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma2', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma3', '', '0', 'o.a', 'hello')
  await client.redis.selva_modify('ma4', '', '0', 'o.a', 'hello')

  // Create edges
  await client.redis.selva_modify('root', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma1', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma3']),
    $delete: null,
    $value: null,
  }))
  await client.redis.selva_modify('ma2', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma4']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'bfs', 'a.b', 'fields', 'a.b\nparents', 'root'),
    [
      [
        'root', [
          'a.b', ['ma1', 'ma2'],
          'parents', [],
        ]
      ],
      [
        'ma1', [
          'a.b', ['ma3'],
          'parents', ['root'],
        ],
      ],
      [
        'ma2', [
          'a.b', ['ma4'],
          'parents', ['root'],
        ],
      ],
      [
        'ma3', [
          'parents', ['root'],
        ]
      ],
      [
        'ma4', [
          'parents', ['root'],
        ],
      ],
    ]
  )
})

test.serial('missing edges are added automatically', async (t) => {
  const client = connect({ port })

  // Create nodes
  console.log(await client.redis.selva_modify('root', '', '0', 'o.a', 'hello'));

  await client.redis.selva_modify('root', '', '5', 'a.b', createRecord(setRecordDefCstring, {
    op_set_type: 1,
    delete_all: 0,
    constraint_id: 0,
    $add: toCArr(['ma1', 'ma2']),
    $delete: null,
    $value: null,
  }))

  t.deepEqual(
    await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'bfs', 'a.b', 'root'),
    [ 'root', 'ma1', 'ma2' ]
  )
})
