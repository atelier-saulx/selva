import test from 'ava'
import { readString, readValue } from 'data-record'
import './assertions'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { idExists } from './assertions'
import getPort from 'get-port'
import { wait } from '../src/util'
import { doubleDef, longLongDef } from '../src/set/modifyDataRecords'

const DEFAULT_HIERARCHY = '___selva_hierarchy'

let srv
let port: number

export function readDouble(x) {
  return readValue(doubleDef, x, '.d')
}

function readLongLong(x) {
  return readValue(longLongDef, x, '.d')
}

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect(
    {
      port,
    },
    { loglevel: 'info' }
  )

  // await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'nl', 'de'],
    rootType: {
      fields: { value: { type: 'number' }, hello: { type: 'url' } },
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          value: { type: 'number' },
          title: {
            type: 'text',
          },
          obj: {
            type: 'object',
            properties: {
              hello: { type: 'string' },
              hallo: { type: 'string' },
              num: { type: 'number' },
            },
          },
          nestedObj: {
            type: 'object',
            properties: {
              a: {
                type: 'object',
                properties: {
                  value: { type: 'string' },
                },
              },
              b: {
                type: 'object',
                properties: {
                  value: { type: 'string' },
                },
              },
            },
          },
          settySet: {
            type: 'set',
            items: {
              type: 'string',
            },
          },
          reffyRefs: {
            type: 'references',
          },
          reffyRef: {
            type: 'reference',
          },
        },
      },
      league: {
        prefix: 'cu',
        fields: {
          title: {
            type: 'text',
          },
        },
      },
      person: {
        prefix: 'pe',
        fields: {
          title: {
            type: 'text',
          },
        },
      },
      someTestThing: {
        prefix: 'vi',
        fields: {
          title: {
            type: 'text',
          },
          value: {
            type: 'number',
          },
        },
      },
      otherTestThing: {
        prefix: 'ar',
        fields: {
          title: {
            type: 'text',
          },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      lekkerType: {
        prefix: 'lk',
        fields: {
          strRec: {
            type: 'record',
            values: {
              type: 'string',
            },
          },
          textRec: {
            type: 'record',
            values: {
              type: 'text',
            },
          },
          objRec: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                floatArray: { type: 'array', items: { type: 'float' } },
                intArray: { type: 'array', items: { type: 'int' } },
                strArray: { type: 'array', items: { type: 'string' } },
                objArray: {
                  type: 'array',
                  items: {
                    type: 'object',
                    properties: {
                      hello: { type: 'string' },
                      value: { type: 'int' },
                    },
                  },
                },
                hello: {
                  type: 'string',
                },
                nestedRec: {
                  type: 'record',
                  values: {
                    type: 'object',
                    properties: {
                      value: {
                        type: 'number',
                      },
                      hello: {
                        type: 'string',
                      },
                    },
                  },
                },
                value: {
                  type: 'number',
                },
                stringValue: {
                  type: 'string',
                },
              },
            },
          },
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } },
              texty: { type: 'text' },
              dung: { type: 'number' },
              dang: {
                type: 'object',
                properties: {
                  dung: { type: 'number' },
                  dunk: { type: 'string' },
                },
              },
              dunk: {
                type: 'object',
                properties: {
                  ding: { type: 'number' },
                  dong: { type: 'number' },
                },
              },
            },
          },
          dong: { type: 'json' },
          dingdongs: { type: 'array', items: { type: 'string' } },
          floatArray: { type: 'array', items: { type: 'float' } },
          intArray: { type: 'array', items: { type: 'int' } },
          refs: { type: 'references' },
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
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

test.serial('createdAt is set', async (t) => {
  const client = connect({
    port,
  })

  const before = Date.now()
  const match = await client.set({
    $language: 'en',
    type: 'match',
    title: 'yesh',
  })
  await wait(2)
  const after = Date.now()

  const result = await client.get({
    $language: 'en',
    $id: match,
    id: true,
    title: true,
    createdAt: true,
    updatedAt: true,
  })

  const {createdAt, updatedAt} = result
  delete result.createdAt
  delete result.updatedAt

  t.deepEqual(result, {
    id: match,
    title: 'yesh',
  })

  t.true(
    typeof createdAt === 'number' && createdAt <= after && createdAt >= before
  )
  t.true(
    typeof updatedAt === 'number' && updatedAt <= after && updatedAt >= before
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('createdAt+updatedAt are set', async (t) => {
  const client = connect({
    port,
  })

  const before = Date.now()
  const person = await client.set({
    $language: 'en',
    type: 'person',
    title: 'yesh',
  })
  const after = Date.now()

  const result = await client.get({
    $language: 'en',
    $id: person,
    id: true,
    title: true,
    createdAt: true,
    updatedAt: true,
  })

  const createdAt = result.createdAt
  const updatedAt = result.updatedAt
  delete result.createdAt
  delete result.updatedAt

  t.deepEqual(result, {
    id: person,
    title: 'yesh',
  })

  t.true(
    typeof createdAt === 'number' && createdAt <= after && createdAt >= before
  )

  t.deepEqual(createdAt, updatedAt)

  await client.delete('root')
  await client.destroy()
})

test.serial('createdAt not set if provided in modify props', async (t) => {
  const client = connect({
    port,
  })

  const match = await client.set({
    $language: 'en',
    type: 'match',
    title: 'yesh',
    createdAt: 12345,
  })

  const result = await client.get({
    $language: 'en',
    $id: match,
    id: true,
    title: true,
    createdAt: true,
  })

  t.deepEqual(result, {
    id: match,
    title: 'yesh',
    createdAt: 12345,
  })

  await client.delete('root')
  await client.destroy()
})

test.serial('createdAt not set if nothing changed', async (t) => {
  const client = connect({
    port,
  })

  const before = Date.now()
  const person = await client.set({
    $language: 'en',
    type: 'person',
    title: 'yesh',
  })
  const after = Date.now()

  let result = await client.get({
    $language: 'en',
    $id: person,
    id: true,
    title: true,
    createdAt: true,
    updatedAt: true,
  })

  let createdAt = result.createdAt
  let updatedAt = result.updatedAt
  delete result.createdAt
  delete result.updatedAt

  t.deepEqual(result, {
    id: person,
    title: 'yesh',
  })

  t.true(
    typeof createdAt === 'number' && createdAt <= after && createdAt >= before
  )

  t.deepEqual(createdAt, updatedAt)

  await client.set({
    $language: 'en',
    type: 'person',
    title: 'yesh',
    children: [],
  })

  result = await client.get({
    $language: 'en',
    $id: person,
    id: true,
    title: true,
    createdAt: true,
    updatedAt: true,
  })

  createdAt = result.createdAt
  updatedAt = result.updatedAt
  delete result.createdAt
  delete result.updatedAt

  t.true(
    typeof createdAt === 'number' && createdAt <= after && createdAt >= before
  )

  t.deepEqual(createdAt, updatedAt)

  await client.delete('root')
  await client.destroy()
})

test.serial('automatic child creation and timestamps', async (t) => {
  const now = Date.now()
  const client = connect(
    {
      port,
    },
    { loglevel: 'info' }
  )

  const parent = await client.set({
    $id: 'viParent',
    title: {
      nl: 'nl',
    },
    children: [
      {
        type: 'match',
        title: {
          nl: 'child1',
        },
      },
      {
        type: 'match',
        title: {
          nl: 'child2',
        },
      },
      {
        type: 'match',
        title: {
          nl: 'child3',
        },
      },
    ],
  })

  const {children} = await client.get({
    $id: 'viParent',
    children: true,
  })

  for (const child of children) {
      const {createdAt, updatedAt} = await client.get({ $id: child, createdAt: true, updatedAt: true })

      t.true(createdAt >= now)
      t.true(updatedAt >= now)
  }

  await client.destroy()
})
