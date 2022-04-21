import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

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
          name: { type: 'string' },
          thing: { type: 'string' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string' },
          description: { type: 'text' },
          value: {
            type: 'number',
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
          status: { type: 'number' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema

  await wait(100)

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('sub find - list with wildcard', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    $id: 'ma1',
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
    },
  })

  await client.set({
    $id: 'ma2',
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
    },
  })

  const obs = client.observe({
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
  })

  let cnt = 0
  obs.subscribe((v) => {
    if (cnt === 0) {
      t.deepEqualIgnoreOrder(v, {
        id: 'root',
        items: [
          {
            name: 'match 1',
            record: {
              obj: { a: 'abba', b: 'babba' },
            },
          },
          {
            name: 'match 2',
            record: {
              obj: { a: '2_abba', b: '2_babba' },
            },
          },
        ],
      })
    } else if (cnt === 1) {
      t.deepEqualIgnoreOrder(v, {
        id: 'root',
        items: [
          {
            name: 'match 1',
            record: {
              obj: { a: 'abba', b: 'babba' },
              newObj: { a: 'new yes' },
            },
          },
          {
            name: 'match 2',
            record: {
              obj: { a: '2_abba', b: '2_babba' },
            },
          },
        ],
      })
    } else {
      t.fail()
    }

    cnt++
  })

  await wait(1e3)

  t.deepEqual(cnt, 1)

  const [sid] = await client.redis.selva_subscriptions_list(
    '___selva_hierarchy'
  )
  console.info(
    'SUB',
    await client.redis.selva_subscriptions_debug('___selva_hierarchy', sid)
  )

  await client.set({
    $id: 'ma1',
    record: {
      newObj: {
        a: 'new yes',
      },
    },
  })

  await wait(1e3)

  t.deepEqual(cnt, 2)

  await client.delete('root')
  await client.destroy()
})

test.serial('sub find - single with wildcard', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    $id: 'ma1',
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
    },
  })

  const obs = client.observe({
    $id: 'ma1',
    id: true,
    name: true,
    record: {
      '*': {
        a: true,
        b: true,
      },
    },
  })

  let cnt = 0
  obs.subscribe((v) => {
    if (cnt === 0) {
      t.deepEqualIgnoreOrder(v, {
        id: 'ma1',
        name: 'match 1',
        record: {
          obj: { a: 'abba', b: 'babba' },
        },
      })
    } else if (cnt === 1) {
      t.deepEqualIgnoreOrder(v, {
        id: 'ma1',
        name: 'match 1',
        record: {
          obj: { a: 'abba', b: 'babba' },
          newObj: { a: 'new yes' },
        },
      })
    } else {
      t.fail()
    }

    cnt++
  })

  await wait(1e3)

  t.deepEqual(cnt, 1)

  await client.set({
    $id: 'ma1',
    record: {
      newObj: {
        a: 'new yes',
      },
    },
  })

  await wait(1e3)

  t.deepEqual(cnt, 2)

  await client.delete('root')
  await client.destroy()
})
