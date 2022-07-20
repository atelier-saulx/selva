import test from 'ava'
import { compile, createRecord } from 'data-record'
import { connect } from '../src/index'
import { setRecordDefCstring, edgeMetaDef } from '../src/set/modifyDataRecords'
import { start } from '@saulx/selva-server'
import redis, { RedisClient } from '@saulx/redis-client'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'
import { joinIds } from '../src/util'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: {
        friends: {
          type: 'record',
          values: { type: 'reference' },
        },
        groups: {
          type: 'record',
          values: {
            type: 'object',
            properties: {
              name: { type: 'string' },
              members: { type: 'references' },
            },
          },
        },
      },
    },
    types: {
      friend: {
        prefix: 'fr',
        fields: {
          name: { type: 'string' },
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

test.serial.failing('references in a record', async (t) => {
  const client = connect({ port })

  const fr1 = await client.set({
    type: 'friend',
    name: 'Cohen Gilliam',
  })
  const fr2 = await client.set({
    type: 'friend',
    name: 'Mohamad Bentley',
  })
  const fr3 = await client.set({
    type: 'friend',
    name: 'Letitia Fitzgerald',
  })

  await client.set({
    $id: 'root',
    friends: {
      cohen: fr1,
      maometto: fr2,
      fitz: fr3,
    },
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      friends: true,
    }),
    {
      friends: {
        cohen: fr1,
        maometto: fr2,
        fitz: fr3,
      }
    }
  )

  client.destroy()
})

test.serial.failing('object record with references', async (t) => {
  const client = connect({ port })

  const fr1 = await client.set({
    type: 'friend',
    name: 'Cohen Gilliam',
  })
  const fr2 = await client.set({
    type: 'friend',
    name: 'Mohamad Bentley',
  })
  const fr3 = await client.set({
    type: 'friend',
    name: 'Letitia Fitzgerald',
  })

  await client.set({
    $id: 'root',
    groups: {
      a: {
        name: 'best friends',
        members: [fr1, fr2],
      },
      b: {
        name: 'worst friends',
        members: [fr3],
      },
    },
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      groups: true,
    }),
    {
      groups: {
        a: {
          name: 'best friends',
          members: [fr1, fr2],
        },
        b: {
          name: 'worst friends',
          members: [fr3],
        },
      }
    }
  )

  client.destroy()
})
