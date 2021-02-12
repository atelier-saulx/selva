import test from 'ava'
import './assertions'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { dumpDb } from './assertions'
import getPort from 'get-port'
import { wait } from '../src/util'

const DEFAULT_HIERARCHY = '___selva_hierarchy'

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
  const client = connect(
    {
      port,
    },
    { loglevel: 'info' }
  )

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'nl', 'de'],
    rootType: {
      fields: { value: { type: 'number' }, title: { type: 'text' } },
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text',
          },
          createdAt: { type: 'timestamp' },
          value: { type: 'number' },
        },
      },
      league: {
        prefix: 'cu',
        fields: {
          title: {
            type: 'text',
          },
          createdAt: { type: 'number' },
        },
      },
      person: {
        prefix: 'pe',
        fields: {
          title: {
            type: 'text',
          },
          createdAt: { type: 'timestamp' },
          updatedAt: { type: 'timestamp' },
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
          things: { type: 'set', items: { type: 'string' } },
          otherThings: { type: 'set', items: { type: 'string' } },
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
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('can delete root', async (t) => {
  const client = connect(
    {
      port,
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match',
  })

  const root = await client.set({
    $id: 'root',
    value: 9001,
  })

  t.deepEqual(root, 'root')
  t.deepEqual(await client.redis.selva_object_get('root', 'value'), '9001')
  t.deepEqual(
    await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'),
    [match]
  )

  await client.delete('root')
  t.deepEqual(await dumpDb(client), [['root', ['id', 'root', 'type', 'root']]])

  await client.destroy()
})

test.serial('can delete a set', async (t) => {
  const client = connect(
    {
      port,
    },
    { loglevel: 'info' }
  )

  const vi = await client.set({
    type: 'someTestThing',
    title: {
      en: 'yes text',
      de: 'ja text',
    },
    value: 5,
    things: ['a', 'b', 'c'],
    otherThings: ['x', 'y', 'z'],
  })
  t.deepEqual(1, 1)

  // That's it, there is nothing more to check as sets are embedded in SelvaObjects
})
