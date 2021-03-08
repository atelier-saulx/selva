import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          thing: { type: 'string', search: { type: ['EXISTS'] } },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          description: { type: 'text' },
          value: {
            type: 'number',
            search: { type: ['NUMERIC', 'SORTABLE', 'EXISTS'] },
          },
          record: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                a: {
                  type: 'string',
                },
                b: {
                  type: 'string',
                },
                nestedRecord: {
                  type: 'record',
                  values: {
                    type: 'object',
                    properties: {
                      a: {
                        type: 'string',
                      },
                      b: {
                        type: 'string',
                      },
                    },
                  },
                },
              },
            },
          },
          status: { type: 'number', search: { type: ['NUMERIC'] } },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - with wildcard', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    value: 1,
    record: {
      obj: {
        a: 'abba',
        b: 'babba',
        nestedRecord: {
          hello: {
            a: 'abba',
            b: 'babba',
          },
          yellow: {
            a: 'abba2',
            b: 'babba2',
          },
        },
      },
      obj2: {
        a: 'abba2',
        b: 'babba2',
        nestedRecord: {
          hello: {
            a: '-abba',
            b: '-babba',
          },
          yellow: {
            a: '-abba2',
            b: '-babba2',
          },
        },
      },
    },
  })

  await client.set({
    type: 'match',
    name: 'match 2',
    value: 2,
    record: {
      obj: {
        a: '2_abba',
        b: '2_babba',
        nestedRecord: {
          hello: {
            a: '2_abba',
            b: '2_babba',
          },
          yellow: {
            a: '2_abba2',
            b: '2_babba2',
          },
        },
      },
      obj2: {
        a: '2_abba2',
        b: '2_babba2',
        nestedRecord: {
          hello: {
            a: '2_-abba',
            b: '2_-babba',
          },
          yellow: {
            a: '2_-abba2',
            b: '2_-babba2',
          },
        },
      },
    },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        record: {
          '*': {
            a: true,
            b: true,
          },
        },
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
            ],
          },
        },
      },
    }),
    {
      id: 'root',
      items: [
        {
          name: 'match 1',
          record: {
            obj: { a: 'abba', b: 'babba' },
            obj2: { a: 'abba2', b: 'babba2' },
          },
        },
        {
          name: 'match 2',
          record: {
            obj: { a: '2_abba', b: '2_babba' },
            obj2: { a: '2_abba2', b: '2_babba2' },
          },
        },
      ],
    }
  )

  await client.delete('root')
  await client.destroy()
})
