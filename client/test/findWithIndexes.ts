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
          cat: { type: 'int' },
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
    'root.I.ImxlIiBl',
    'not_active',
  ])

  await wait(1e3)

  t.deepEqual((await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]), [
    'root.I.ImxlIiBl',
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
    'root.I.ImxlIiBl',
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

test.serial('find index string sets', async (t) => {
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

test.serial('find index integers', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  for (let i = 0; i < 1000; i++) {
    await client.set({
      type: 'league',
      name: `League ${i}`,
      cat: i % 10,
    })
  }

  const q = async () => await client.get({
    $id: 'root',
    id: true,
    items: {
      name: true,
      cat: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'cat',
              $operator: '=',
              $value: 3,
            },
          ],
        },
      },
    },
  })

  for (let i = 0; i < 500; i++) {
    await q()
  }
  await wait(1e3)
  for (let i = 0; i < 500; i++) {
    await q()
  }

  t.deepEqual(
    await client.redis.selva_index_list('___selva_hierarchy'),
    [ 'root.I.ImNhdCIgZyAjMyBG', [ 100, 1001, 100, 100 ] ]
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('find index exists', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  for (let i = 0; i < 1000; i++) {
    const o = {
      type: 'league',
      name: `League ${i}`,
      thing: 'abc',
      things: ['a', 'b']
    }
    if (i % 2) {
      delete o.thing
    } else {
      delete o.things
    }
    await client.set(o)
  }

  const q1 = async () => client.get({
    $id: 'root',
    id: true,
    items: {
      name: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'thing',
              $operator: 'exists',
            },
          ],
        },
      },
    },
  })
  const q2 = async () => client.get({
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
              $operator: 'notExists',
            },
          ],
        },
      },
    },
  })

  for (let i = 0; i < 500; i++) {
    await q1()
  }
  for (let i = 0; i < 500; i++) {
    await q2()
  }
  await wait(2e3)
  for (let i = 0; i < 500; i++) {
    await q1()
  }
  for (let i = 0; i < 500; i++) {
    await q2()
  }
  await wait(2e3)

  t.deepEqual(
    await client.redis.selva_index_list('___selva_hierarchy'),
    [
      'root.I.InRoaW5nIiBo', [ 500, 1001, 500, 500 ],
      'root.I.InRoaW5ncyIgaCBM', [ 500, 1001, 500, 500 ]
    ]
  )

  await client.delete('root')
  await client.destroy()
})
