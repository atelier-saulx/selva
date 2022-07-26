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
        info: {
          type: 'record',
          values: {
            type: 'object',
            properties: {
              kind: { type: 'string' },
              ref: { type: 'reference' },
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

test.serial('references in a record', async (t) => {
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

test.serial('object record with references', async (t) => {
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
      c: {
        name: 'empty',
      },
    },
  })

  t.deepEqualIgnoreOrder(
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
        c: {
          name: 'empty',
        },
      }
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      groups: {
        '*': { name: true },
      },
    }),
    {
      groups: {
        a: {
          name: 'best friends',
        },
        b: {
          name: 'worst friends',
        },
        c: {
          name: 'empty',
        },
      }
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      groups: {
        '*': { members: true },
      }
    }),
    {
      groups: {
        a: {
          members: [fr1, fr2],
        },
        b: {
          members: [fr3],
        },
      }
    }
  )

  client.destroy()
})

test.serial('single references in an object record', async (t) => {
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
    info: {
      cohen: {
        kind: 'good',
        ref: fr1,
      },
      maometto: {
        kind: 'bad',
        ref: fr2,
      },
      fitz: {
        kind: 'best',
        ref: fr3,
      }
    },
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      info: true,
    }),
    {
      info: {
        cohen: {
          kind: 'good',
          ref: fr1,
        },
        maometto: {
          kind: 'bad',
          ref: fr2,
        },
        fitz: {
          kind: 'best',
          ref: fr3,
        }
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'root',
      info: {
        '*': {
          ref: true,
        },
      },
    }),
    {
      info: {
        cohen: {
          ref: fr1,
        },
        maometto: {
          ref: fr2,
        },
        fitz: {
          ref: fr3,
        }
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'root',
      info: {
        '*': {
          kind: true,
        },
      },
    }),
    {
      info: {
        cohen: {
          kind: 'good',
        },
        maometto: {
          kind: 'bad',
        },
        fitz: {
          kind: 'best',
        }
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'root',
      info: {
        '*': {
          kind: true,
          ref: true,
        },
      },
    }),
    {
      info: {
        cohen: {
          kind: 'good',
          ref: fr1,
        },
        maometto: {
          kind: 'bad',
          ref: fr2,
        },
        fitz: {
          kind: 'best',
          ref: fr3,
        }
      },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'root',
      info: {
        '*': {
          kind: true,
          ref: { name: true },
        },
      },
    }),
    {
      info: {
        cohen: {
          kind: 'good',
          ref: { name: 'Cohen Gilliam' },
        },
        maometto: {
          kind: 'bad',
          ref: { name: 'Mohamad Bentley' },
        },
        fitz: {
          kind: 'best',
          ref: { name: 'Letitia Fitzgerald' },
        }
      },
    }
  )

  client.destroy()
})
