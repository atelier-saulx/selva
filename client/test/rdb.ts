import test from 'ava'
import { join } from 'path'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { removeDump } from './assertions'
import getPort from 'get-port'
import { wait } from '../src/util'

const dir = join(process.cwd(), 'tmp', 'rdb-test')
let srv
let port: number

async function restartServer() {
  if (srv) {
    srv.destroy()
    await wait(5000)
  }
  removeDump(dir)

  port = await getPort()
  srv = await start({
    port,
    dir,
  })
}

test.after(removeDump(dir))

test.beforeEach(async (t) => {
  await restartServer()

  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
  const client = connect({ port })
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
      lekkerType: {
        prefix: 'vi',
        fields: {
          title: { type: 'text' },
          strRec: {
            type: 'record',
            values: {
              type: 'string',
            },
          },
          stringAry: {
            type: 'array',
            items: {
              type: 'string',
            },
          },
          intAry: {
            type: 'array',
            items: {
              type: 'int',
            },
          },
          doubleAry: {
            type: 'array',
            items: {
              type: 'float',
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
                hello: {
                  type: 'string',
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
  removeDump(dir)
})

test.serial('can reload from RDB', async (t) => {
  let client = connect({ port })

  await client.set({
    $id: 'viTest',
    title: { en: 'hello' },
    stringAry: ['hello', 'world'],
    doubleAry: [1.0, 2.1, 3.2],
    intAry: [7, 6, 5, 4, 3, 2, 999],
  })

  await client.redis.save()
  await wait(1000)
  await restartServer()
  await client.destroy()
  await wait(5000)
  client = connect({ port })

  t.deepEqualIgnoreOrder(
    await client.get({ $id: 'viTest', $all: true, parents: true }),
    {
      id: 'viTest',
      type: 'lekkerType',
      parents: ['root'],
      title: { en: 'hello' },
      stringAry: ['hello', 'world'],
      doubleAry: [1.0, 2.1, 3.2],
      intAry: [7, 6, 5, 4, 3, 2, 999],
    }
  )
})
