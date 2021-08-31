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
          things: { type: 'set', items: { type: 'string' } },
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

test.serial('find index', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    type: 'league',
    name: 'league 1',
  })

  await client.set({
    type: 'league',
    name: 'league 2',
    thing: 'yes some value here',
  })

  for (let i = 0; i < 500; i++) {
    t.deepEqualIgnoreOrder(
      await client.get({
        $id: 'root',
        id: true,
        items: {
          name: true,
          $list: {
            $find: {
              $traverse: 'descendants',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'league',
                },
                {
                  $field: 'thing',
                  $operator: 'exists',
                },
              ],
            },
          },
        },
      }),
      { id: 'root', items: [{ name: 'league 2' }] }
    )
  }

  t.deepEqual((await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]), [
    'root.I.ImxlYWd1ZSIgZQ==',
    'not_active',
  ])

  await wait(1e3)

  t.deepEqual((await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]), [
    'root.I.ImxlYWd1ZSIgZQ==',
    'not_active',
  ])

  for (let i = 0; i < 1000; i++) {
    await client.set({
      type: 'league',
      name: 'league 2',
      thing: 'yes some value here',
    })
  }

  for (let i = 0; i < 500; i++) {
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'league',
              },
              {
                $field: 'thing',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    })
  }
  await wait(2e3);
  for (let i = 0; i < 500; i++) {
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'league',
              },
              {
                $field: 'thing',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    })
  }

  t.deepEqual((await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]), [
    'root.I.ImxlYWd1ZSIgZQ==',
    1002,
  ])

  await client.delete('root')
  await client.destroy()
})

test.serial('find index strings', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  for (let i = 0; i < 10000; i++) {
    const le = {
      type: 'league',
      name: `league ${i % 3}`,
      thing: 'yeeeesssshhh',
    }

    if (i % 2 == 0) {
      delete le.thing
    }

    await client.set(le)
  }

  await client.redis.selva_index_new('___selva_hierarchy', 'descendants', '', 'root', '"name" f "league 0" c')
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $field: 'name',
                $operator: '=',
                $value: 'league 0',
              },
              {
                $field: 'thing',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    })
  }

  t.deepEqual(
    await client.redis.selva_index_list('___selva_hierarchy'),
    ['root.I.Im5hbWUiIGYgImxlYWd1ZSAwIiBj', [ 0, 101, 1667, 3334 ]]
  )

  await client.delete('root')
  await client.destroy()
})


test.serial.only('find index string sets', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  for (let i = 0; i < 1000; i++) {
    await client.set({
      type: 'league',
      name: `League ${i}`,
      thing: 'abc',
      things: i % 100 != 0 ? ['a', 'b', 'c', 'd', 'e', 'f', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'] : ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'],
    })
  }

  await client.redis.selva_index_new('___selva_hierarchy', 'descendants', '', 'root', '"g" "things" a')
  await client.redis.selva_index_new('___selva_hierarchy', 'descendants', '', 'root', '"thing" f "abc" c')
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $field: 'things',
                $operator: 'has',
                $value: 'g',
              },
            ],
          },
        },
      },
    })
  }
  await wait(1e3)
  for (let i = 0; i < 500; i++) {
    const r = await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $field: 'things',
                $operator: 'has',
                $value: 'g',
              },
              {
                $field: 'thing',
                $operator: '=',
                $value: 'abc',
              },
            ],
          },
        },
      },
    })
  }
  await wait(2e3)

  t.deepEqual(
    await client.redis.selva_index_list('___selva_hierarchy'),
    [
      'root.I.ImciICJ0aGluZ3MiIGE=',
      [ 0, 101, 10, 10 ],
      'root.I.InRoaW5nIiBmICJhYmMiIGM=',
      [ 0, 101, 10, 1000 ]
    ]
  )

  await client.delete('root')
  await client.destroy()
})
