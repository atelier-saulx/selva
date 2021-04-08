import { promisify } from 'util'
import { join } from 'path'
import test from 'ava'
import { connect } from '../src/index'
import { createRecord } from 'data-record'
import { start, startReplica } from '@saulx/selva-server'
import redis, { RedisClient, ReplyError } from 'redis'
import './assertions'
import { wait, removeDump } from './assertions'
import getPort from 'get-port'
import {
  doubleDef,
  incrementDef,
  incrementDoubleDef,
  longLongDef,
  OPT_SET_TYPE,
  setRecordDefCstring,
  setRecordDefDouble,
  setRecordDefInt64
} from '../src/set/modifyDataRecords'

const SELVA_MODIFY_ARG_DEFAULT_STRING = '2'; // Set a string value if unset.
const SELVA_MODIFY_ARG_STRING = '0'; // Value is a string.
const SELVA_MODIFY_ARG_STRING_ARRAY = '6'; // Array of C-strings.
const SELVA_MODIFY_ARG_DEFAULT_LONGLONG = '8';
const SELVA_MODIFY_ARG_LONGLONG = '3'; // Value is a long long.
const SELVA_MODIFY_ARG_DEFAULT_DOUBLE = '9';
const SELVA_MODIFY_ARG_DOUBLE = 'A'; // Value is a double.
const SELVA_MODIFY_ARG_OP_INCREMENT = '4'; // Increment a long long value.
const SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE = 'B'; // Increment a double value.
const SELVA_MODIFY_ARG_OP_SET = '5'; // Value is a struct SelvaModify_OpSet.
const SELVA_MODIFY_ARG_OP_DEL = '7'; // Delete field; value is a modifier.
const SELVA_MODIFY_ARG_OP_OBJ_META = 'C'; // Set object user metadata.

let srv
let port: number
let replica
let rclientOrigin: RedisClient | null = null
let rclientReplica: RedisClient | null = null

const dir = join(process.cwd(), 'tmp', 'replication')

test.before(async (t) => {
  await removeDump(dir)()

  port = await getPort()
  srv = await start({
    port,
    dir: join(dir, 'srv'),
  })

  replica = await startReplica({
    registry: { port },
    default: true,
    dir: join(dir, 'replica'),
  })
  replica.on('stderr', (b) => console.log(b.toString()))
  replica.on('stdout', (b) => console.log(b.toString()))
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall({ type: 'origin' })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        value1: { type: 'number' },
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

  rclientOrigin = redis.createClient(replica.origin.port)
  rclientReplica = redis.createClient(replica.port)
})

test.after(async (_t) => {
  const client = connect({ port })
  await client.destroy()
  await replica.destroy()
  await srv.destroy()
  await removeDump(dir)()
})

test.afterEach(async () => {
  if (rclientOrigin) {
    rclientOrigin.end(true)
    rclientOrigin = null
    rclientReplica.end(true)
    rclientReplica = null
  }
})

test.serial('verify basic replication', async (t) => {
  const infoOrigin = await new Promise((resolve, reject) =>
    rclientOrigin.info((err, res) => (err ? reject(err) : resolve(res)))
  )
  const infoReplica = await new Promise((resolve, reject) =>
    rclientReplica.info((err, res) => (err ? reject(err) : resolve(res)))
  )

  t.assert(
  // @ts-ignore
    infoOrigin.includes('role:master'),
    'Origin has the correct Redis role'
  )
  t.assert(
  // @ts-ignore
    infoReplica.includes('role:slave'),
    'Replica has the correct Redis role'
  )

  await new Promise((resolve, reject) =>
    rclientOrigin.set('xyz', '1', (err, res) =>
      err ? reject(err) : resolve(res)
    )
  )
  await wait(20)
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientReplica.get('xyz', (err, res) =>
        err ? reject(err) : resolve(res)
      )
    ),
    '1'
  )
})

test.serial('hierarchy replication', async (t) => {
  //await new Promise((resolve, _reject) => {
  //  setTimeout(resolve, 30000)
  //})
  t.deepEqual(await Promise.all([
      new Promise((resolve, reject) =>
        rclientOrigin.send_command(
          'selva.modify',
          ['grphnode_a', '', '0', 'title.en', 'lol'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      new Promise((resolve, reject) =>
        rclientOrigin.send_command(
          'selva.modify',
          ['grphnode_b', '', '0', 'title.en', 'lol'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
    ]),
    [['grphnode_a', 'UPDATED'], ['grphnode_b', 'UPDATED']])
  await wait(20)
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientReplica.send_command(
        'selva.hierarchy.find',
        ['', '___selva_hierarchy', 'bfs', 'descendants', 'root'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['grphnode_a', 'grphnode_b']
  )

  await new Promise((resolve, reject) =>
    rclientOrigin.send_command(
      'selva.hierarchy.del',
      ['___selva_hierarchy', 'grphnode_a'],
      (err, res) => (err ? reject(err) : resolve(res))
    )
  )
  await wait(20)
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientReplica.send_command(
        'selva.hierarchy.find',
        ['', '___selva_hierarchy', 'bfs', 'descendants', 'root'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['grphnode_b']
  )
})

test.serial('modify command is replicated', async (t) => {
  const res1 = await new Promise((resolve, reject) =>
    rclientOrigin.send_command(
      'selva.modify',
      ['grphnode_a', '', '0', 'value', '5', '0', 'value1', '100'],
      (err, res) => (err ? reject(err) : resolve(res))
    )
  )
  t.deepEqual(res1, ['grphnode_a', 'UPDATED', 'UPDATED'])
  await wait(200)
  const keys = (await new Promise((resolve, reject) =>
    rclientReplica.keys('*', (err, res) => (err ? reject(err) : resolve(res)))
  )) as string[]
  t.assert(keys.includes('grphnode_a'))
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.object.get',
        ['', 'grphnode_a'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['id', 'grphnode_a', 'value', '5', 'value1', '100']
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientReplica.send_command(
        'selva.object.get',
        ['', 'grphnode_a'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['id', 'grphnode_a', 'value', '5', 'value1', '100']
  )

  // Only one update
  const res2 = await new Promise((resolve, reject) =>
    rclientOrigin.send_command(
      'selva.modify',
      ['grphnode_a', '', '0', 'value', '5', '0', 'value1', '2'],
      (err, res) => (err ? reject(err) : resolve(res))
    )
  )
  t.deepEqual(res2, ['grphnode_a', 'OK', 'UPDATED'])
  await wait(200)
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.object.get',
        ['', 'grphnode_a'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['id', 'grphnode_a', 'value', '5', 'value1', '2']
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientReplica.send_command(
        'selva.object.get',
        ['', 'grphnode_a'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['id', 'grphnode_a', 'value', '5', 'value1', '2']
  )
})

test.serial('modify command is replicated ignoring errors', async (t) => {
  const res1 = await new Promise((resolve, reject) =>
    rclientOrigin.send_command(
      'selva.modify',
      ['grphnode_a', '', '0', 'value', '5', '127', 'value1', '100'],
      (err, res) => (err ? reject(err) : resolve(res))
    )
  )
  t.deepEqual(res1[1], 'UPDATED')
  t.assert(res1[2] instanceof ReplyError)

  await wait(200)

  const keys = (await new Promise((resolve, reject) =>
    rclientReplica.keys('*', (err, res) => (err ? reject(err) : resolve(res)))
  )) as string[]
  t.assert(keys.includes('grphnode_a'))

  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.object.get',
        ['', 'grphnode_a'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['id', 'grphnode_a', 'value', '5']
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientReplica.send_command(
        'selva.object.get',
        ['', 'grphnode_a'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['id', 'grphnode_a', 'value', '5']
  )
})

test.serial('modify all cases are replicated', async (t) => {
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_DEFAULT_STRING, 'f02', 'abc'],
          ...[SELVA_MODIFY_ARG_STRING, 'f04', 'def'],
          ...[SELVA_MODIFY_ARG_DEFAULT_LONGLONG, 'f07', createRecord(longLongDef, { d: BigInt(42) })],
          ...[SELVA_MODIFY_ARG_DEFAULT_DOUBLE, 'f10', createRecord(doubleDef, { d: 13.37 })],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT, 'f13', createRecord(incrementDef, { $default: BigInt(1), $increment: BigInt(1), })],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE, 'f15', createRecord(incrementDoubleDef, { $default: 3.14, $increment: 1.1, })],
          ...[SELVA_MODIFY_ARG_STRING, 'f17', 'to be deleted'],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
      'UPDATED',
      'UPDATED',
      'UPDATED',
      'UPDATED',
      'UPDATED',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_DEFAULT_STRING, 'f01', 'abc'], // set
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f01', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_STRING, 'f02', 'abc'], // already set, not replicated
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f02', '1234'],
          ...[SELVA_MODIFY_ARG_STRING, 'f03', 'def'],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f03', '1234'],
          ...[SELVA_MODIFY_ARG_STRING, 'f04', 'def'], // existing
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f04', '1234'],
          //...[SELVA_MODIFY_ARG_STRING_ARRAY, 'f05', 'abc\0def'], // Only supported for alias
          //...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f05', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_LONGLONG, 'f06', createRecord(longLongDef, { d: BigInt(13) })], // set
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f06', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_LONGLONG, 'f07', createRecord(longLongDef, { d: BigInt(43) })], // already set, not replicated
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f07', '1234'],
          ...[SELVA_MODIFY_ARG_LONGLONG, 'f08', createRecord(longLongDef, { d: BigInt(15) })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f08', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_DOUBLE, 'f09', createRecord(doubleDef, { d: 1.414213562 })], // set
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f09', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_DOUBLE, 'f10', createRecord(doubleDef, { d: 1.414213562 })], // already set, not replicated
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f10', '1234'],
          ...[SELVA_MODIFY_ARG_DOUBLE, 'f11', createRecord(doubleDef, { d: 2.718281828 })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f11', '1234'],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT, 'f12', createRecord(incrementDef, { $default: BigInt(10), $increment: BigInt(10) })], // new value
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f12', '1234'],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT, 'f13', createRecord(incrementDef, { $default: BigInt(11), $increment: BigInt(5) })], // existing value incremented
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f13', '1234'],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE, 'f14', createRecord(incrementDoubleDef, { $default: 0.99, $increment: 1.01, })], // new value
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f14', '1234'],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE, 'f15', createRecord(incrementDoubleDef, { $default: 900.01, $increment: 1.01, })], // existing value incremented
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f15', '1234'],
          ...[SELVA_MODIFY_ARG_OP_SET, 'f16', createRecord(setRecordDefCstring, {
              op_set_type: OPT_SET_TYPE.char,
              delete_all: 0,
              $add: '',
              $delete: '',
              $value: 'lol\0lal',
          })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f16', '1234'],
          ...[SELVA_MODIFY_ARG_OP_DEL, 'f17', ''],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',    // f01
      'UPDATED',    // f01
      'OK',         // f02
      'UPDATED',    // f02
      'UPDATED',    // f03
      'UPDATED',    // f03
      'OK',         // f04
      'UPDATED',    // f04
      //'UPDATED',    // f05
      //'UPDATED',    // f05
      'UPDATED',    // f06
      'UPDATED',    // f06
      'OK',         // f07
      'UPDATED',    // f07
      'UPDATED',    // f08
      'UPDATED',    // f08
      'UPDATED',    // f09
      'UPDATED',    // f09
      'OK',         // f10
      'UPDATED',    // f10
      'UPDATED',    // f11
      'UPDATED',    // f11
      'UPDATED',    // f12
      'UPDATED',    // f12
      'UPDATED',    // f13
      'UPDATED',    // f13
      'UPDATED',    // f14
      'UPDATED',    // f14
      'UPDATED',    // f15
      'UPDATED',    // f15
      'UPDATED',    // f16
      'UPDATED',    // f16
      'UPDATED',    // f17
    ]
  )


  await wait(200)

  const keys = (await new Promise((resolve, reject) =>
    rclientReplica.keys('*', (err, res) => (err ? reject(err) : resolve(res)))
  )) as string[]
  t.assert(keys.includes('grphnode_a'))

  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', 'abc'],
        ...['f02', 'abc'],
        ...['f03', 'def'],
        ...['f04', 'def'],
        ...['f06', 13],
        ...['f07', 42],
        ...['f08', 15],
        ...['f09', '1.414213562'],
        ...['f10', '13.369999999999999'],
        ...['f11', '2.7182818279999998'],
        ...['f12', 10],
        ...['f13', 6],
        ...['f14', '0.98999999999999999'],
        ...['f15', '4.1500000000000004'],
        ...['f16', ['lal', 'lol']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_DEFAULT_STRING, 'f01', 'abc'],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f01', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_STRING, 'f02', 'abc'],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f02', '1234'],
          ...[SELVA_MODIFY_ARG_STRING, 'f03', 'def'],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f03', '1234'],
          ...[SELVA_MODIFY_ARG_STRING, 'f04', 'def'],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f04', '1234'],
          //...[SELVA_MODIFY_ARG_STRING_ARRAY, 'f05', 'abc\0def'], // Only supported for alias
          //...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f05', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_LONGLONG, 'f06', createRecord(longLongDef, { d: BigInt(13) })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f06', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_LONGLONG, 'f07', createRecord(longLongDef, { d: BigInt(43) })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f07', '1234'],
          ...[SELVA_MODIFY_ARG_LONGLONG, 'f08', createRecord(longLongDef, { d: BigInt(15) })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f08', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_DOUBLE, 'f09', createRecord(doubleDef, { d: 1.414213562 })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f09', '1234'],
          ...[SELVA_MODIFY_ARG_DEFAULT_DOUBLE, 'f10', createRecord(doubleDef, { d: 1.414213562 })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f10', '1234'],
          ...[SELVA_MODIFY_ARG_DOUBLE, 'f11', createRecord(doubleDef, { d: 2.718281828 })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f11', '1234'],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT, 'f12', createRecord(incrementDef, { $default: BigInt(10), $increment: BigInt(10) })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f12', '1234'],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT, 'f13', createRecord(incrementDef, { $default: BigInt(11), $increment: BigInt(5) })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f13', '1234'],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE, 'f14', createRecord(incrementDoubleDef, { $default: 0.99, $increment: 1.01, })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f14', '1234'],
          ...[SELVA_MODIFY_ARG_OP_INCREMENT_DOUBLE, 'f15', createRecord(incrementDoubleDef, { $default: 900.01, $increment: 1.01, })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f15', '1234'],
          ...[SELVA_MODIFY_ARG_OP_SET, 'f16', createRecord(setRecordDefCstring, {
              op_set_type: OPT_SET_TYPE.char,
              delete_all: 0,
              $add: '',
              $delete: '',
              $value: 'lol\0lal',
          })],
          ...[SELVA_MODIFY_ARG_OP_OBJ_META, 'f16', '1234'],
          ...[SELVA_MODIFY_ARG_OP_DEL, 'f17', ''],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',         // f01
      'UPDATED',    // f01
      'OK',         // f02
      'UPDATED',    // f02
      'OK',         // f03
      'UPDATED',    // f03
      'OK',         // f04
      'UPDATED',    // f04
      //'UPDATED',    // f05
      //'UPDATED',    // f05
      'OK',         // f06
      'UPDATED',    // f06
      'OK',         // f07
      'UPDATED',    // f07
      'OK',         // f08
      'UPDATED',    // f08
      'OK',         // f09
      'UPDATED',    // f09
      'OK',         // f10
      'UPDATED',    // f10
      'OK',         // f11
      'UPDATED',    // f11
      'UPDATED',    // f12
      'UPDATED',    // f12
      'UPDATED',    // f13
      'UPDATED',    // f13
      'UPDATED',    // f14
      'UPDATED',    // f14
      'UPDATED',    // f15
      'UPDATED',    // f15
      'OK',         // f16
      'UPDATED',    // f16
      'OK',         // f17
    ]
  )

  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', 'abc'],
        ...['f02', 'abc'],
        ...['f03', 'def'],
        ...['f04', 'def'],
        ...['f06', 13],
        ...['f07', 42],
        ...['f08', 15],
        ...['f09', '1.414213562'],
        ...['f10', '13.369999999999999'],
        ...['f11', '2.7182818279999998'],
        ...['f12', 20],
        ...['f13', 11],
        ...['f14', '2'],
        ...['f15', '5.1600000000000001'],
        ...['f16', ['lal', 'lol']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))
})

test.serial('modify set ops are replicated (cstring)', async (t) => {
  // Create a new set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: '',
            $delete: '',
            $value: 'abc\0def\0ghi',
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: null,
            $delete: null,
            $value: 'abc\0def\0ghi',
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['abc', 'def', 'ghi']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Add to an existing set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: 'xyz',
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: 'xyz',
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['abc', 'def', 'ghi', 'xyz']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Delete from a set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: null,
            $delete: 'def',
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: null,
            $delete: 'def',
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['abc', 'ghi', 'xyz']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Replace values
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: null,
            $delete: null,
            $value: 'hallo',
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['hallo']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // delete_all
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 1,
            $add: null,
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 1,
            $add: null,
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Delete from a non-existing set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: null,
            $delete: 'def',
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Add to a new set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefCstring, {
            op_set_type: OPT_SET_TYPE.char,
            delete_all: 0,
            $add: 'xyz',
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['xyz']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))
})

test.serial.skip('modify set ops are replicated (reference)', async (t) => {
    // TODO Test reference sets
})

test.serial('modify set ops are replicated (double)', async (t) => {
  // Create a new set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: null,
            $delete: null,
            $value: [1.0, 2.0, 3.0],
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: null,
            $delete: null,
            $value: [1.0, 2.0, 3.0],
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['1', '2', '3']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Add to an existing set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: [4.0],
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: [4.0],
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['1', '2', '3', '4']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Delete from a set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: null,
            $delete: [2.0],
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: null,
            $delete: [2.0],
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['1', '3', '4']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Replace values
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: null,
            $delete: null,
            $value: [5.0, 6.0],
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['5', '6']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // delete_all
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 1,
            $add: null,
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 1,
            $add: null,
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Delete from a non-existing set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: null,
            $delete: [1.0],
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Add to a new set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefDouble, {
            op_set_type: OPT_SET_TYPE.double,
            delete_all: 0,
            $add: [13.37],
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', ['13.369999999999999']],
        ...['id', 'grphnode_a'],
      ]
    )
  }))
})

test.serial('modify set ops are replicated (long long)', async (t) => {
  // Create a new set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: null,
            $delete: null,
            $value: [1, 2, 3].map(BigInt),
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: null,
            $delete: null,
            $value: [1, 2, 3].map(BigInt),
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', [1, 2, 3]],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Add to an existing set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: [4].map(BigInt),
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: [4].map(BigInt),
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', [1, 2, 3, 4]],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Delete from a set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: null,
            $delete: [2].map(BigInt),
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: null,
            $delete: [2].map(BigInt),
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', [1, 3, 4]],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Replace values
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: null,
            $delete: null,
            $value: [5, 6].map(BigInt),
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', [5, 6]],
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // delete_all
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 1,
            $add: null,
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 1,
            $add: null,
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Delete from a non-existing set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: null,
            $delete: [1].map(BigInt),
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'OK',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['id', 'grphnode_a'],
      ]
    )
  }))

  // Add to a new set
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.modify',
        [
          'grphnode_a', '',
          ...[SELVA_MODIFY_ARG_OP_SET, 'f01', createRecord(setRecordDefInt64, {
            op_set_type: OPT_SET_TYPE.long_long,
            delete_all: 0,
            $add: [13].map(BigInt),
            $delete: null,
            $value: null,
          })],
        ],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    [
      'grphnode_a',
      'UPDATED',
    ]
  )
  await wait(200)
  await Promise.all([rclientOrigin, rclientReplica].map(async (r) => {
    t.deepEqual(
      await new Promise((resolve, reject) =>
        r.send_command(
          'selva.object.get',
          ['', 'grphnode_a'],
          (err, res) => (err ? reject(err) : resolve(res))
        )
      ),
      [
        ...['f01', [13]],
        ...['id', 'grphnode_a'],
      ]
    )
  }))
})

test.serial.skip('modify $alias is replicated', async (t) => {
    // TODO Test alias replication
})

test.serial('replicate hierarchy parents with modify', async (t) => {
  await wait(5000)
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    $language: 'en',
    $id: 'ma1',
    title: 'match 1',
  })

  //client.observe({
  //  $language: 'en',
  //  children: { title: true, $list: true }
  //}).subscribe((s) => console.log('s1', s))
  //client.observe({
  //  $id: 'ma1',
  //  $language: 'en',
  //  children: { title: true, $list: true }
  //}).subscribe((s) => console.log('s2', s))
  //await wait(100)

  await client.set({
    $language: 'en',
    $id: 'ma2',
    title: 'match 2',
    parents: ['ma1'],
  })
  await client.set({
    $language: 'en',
    $id: 'ma3',
    title: 'match 3',
    parents: ['ma1'],
  })
  await client.set({
    $language: 'en',
    $id: 'ma3',
    title: 'match 3',
    parents: ['ma1'],
  })

  client.destroy()
  await wait(500)

  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientOrigin.send_command(
        'selva.hierarchy.children',
        ['___selva_hierarchy', 'root'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['ma1']
  )
  t.deepEqual(
    await new Promise((resolve, reject) =>
      rclientReplica.send_command(
        'selva.hierarchy.children',
        ['___selva_hierarchy', 'root'],
        (err, res) => (err ? reject(err) : resolve(res))
      )
    ),
    ['ma1']
  )
})
