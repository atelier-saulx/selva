import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait, getIndexingState } from './assertions'
import getPort from 'get-port'

let srv
let port

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
    selvaOptions: [
      'FIND_INDICES_MAX',
      '100',
      'FIND_INDEXING_INTERVAL',
      '1000',
      'FIND_INDEXING_ICB_UPDATE_INTERVAL',
      '500',
      'FIND_INDEXING_POPULARITY_AVE_PERIOD',
      '3',
    ],
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
          things: { type: 'set', items: { type: 'string' } },
          cat: { type: 'int' },
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
          status: { type: 'number' },
        },
      },
    },
  })

  await wait(100)

  await client.destroy()
})

test.afterEach(async (t) => {
  const client = connect({ port: port })
  await wait(100)
  await client.delete('root')
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: port })
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find index', async (t) => {
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

  const q = {
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
  }

  for (let i = 0; i < 500; i++) {
    t.deepEqualIgnoreOrder(await client.get(q), {
      id: 'root',
      items: [{ name: 'league 2' }],
    })
  }

  t.deepEqual(
    (await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) =>
      i % 2 === 0 ? v : v[3]
    ),
    [
      'root.J.ImxlIiBl', 'not_active',
      'root.J.InRoaW5nIiBo', 'not_active'
    ]
  )
  await wait(1e3)
  t.deepEqual(
    (await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) =>
      i % 2 === 0 ? v : v[3]
    ),
    [
     'root.J.ImxlIiBl', 'not_active',
     'root.J.InRoaW5nIiBo', 'not_active'
    ]
  )

  for (let i = 0; i < 1000; i++) {
    await client.set({
      type: 'league',
      name: 'league 2',
      thing: 'yes some value here',
    })
  }
  for (let i = 0; i < 2000; i++) {
    await client.set({
      type: 'league',
      name: 'league 3',
    })
  }

  for (let i = 0; i < 500; i++) {
    await client.get(q)
  }
  await wait(1e3)
  for (let i = 0; i < 500; i++) {
    await client.get(q)
  }

  const istate = await getIndexingState(client)
  t.truthy(istate['root.J.ImxlIiBl'])
  t.truthy(istate['root.J.ImxlIiBl'].take_max_ave > 140, istate['root.J.ImxlIiBl'].take_max_ave)
  t.truthy(istate['root.J.ImxlIiBl'].tot_max_ave > 400, istate['root.J.ImxlIiBl'].tot_max_ave)
  t.truthy(istate['root.J.ImxlIiBl'].ind_take_max_ave < 1, istate['root.J.ImxlIiBl'].ind_take_max_ave)
  t.truthy(istate['root.J.ImxlIiBl'].card === '3002', istate['root.J.ImxlIiBl'].card)
  t.truthy(istate['root.J.InRoaW5nIiBo'])
  t.truthy(istate['root.J.InRoaW5nIiBo'].take_max_ave > 150, istate['root.J.InRoaW5nIiBo'].take_max_ave)
  t.truthy(istate['root.J.InRoaW5nIiBo'].tot_max_ave > 400, istate['root.J.InRoaW5nIiBo'].tot_max_ave)
  t.truthy(istate['root.J.InRoaW5nIiBo'].ind_take_max_ave > 700, istate['root.J.InRoaW5nIiBo'].ind_take_max_ave)
  t.truthy(istate['root.J.InRoaW5nIiBo'].card === '1001', istate['root.J.InRoaW5nIiBo'].card)

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

    if (i % 2 === 0) {
      delete le.thing
    }

    await client.set(le)
  }

  await client.redis.selva_index_new(
    '___selva_hierarchy',
    'descendants',
    '',
    'none',
    '',
    'root',
    '"name" f "league 0" c'
  )
  await wait(2e3)
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

  const ilist = await client.redis.selva_index_list('___selva_hierarchy')
  t.deepEqual(ilist[0], 'root.J.Im5hbWUiIGYgImxlYWd1ZSAwIiBj')
  t.truthy(ilist[1][0] > 80, `${ilist[1][0]}`)
  t.truthy(ilist[1][1] > 80, `${ilist[1][1]}`)
  t.truthy(ilist[1][2] > 1300, `${ilist[1][2]}`)
  t.truthy(ilist[1][3] > 3000, `${ilist[1][2]}`)

  await client.destroy()
})

test.serial('find index string sets', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  for (let i = 0; i < 1000; i++) {
    await client.set({
      type: 'league',
      name: `League ${i}`,
      thing: 'abc',
      things:
        i % 100 !== 0
          ? [
              'a',
              'b',
              'c',
              'd',
              'e',
              'f',
              'h',
              'i',
              'j',
              'k',
              'l',
              'm',
              'n',
              'o',
              'p',
            ]
          : [
              'a',
              'b',
              'c',
              'd',
              'e',
              'f',
              'g',
              'h',
              'i',
              'j',
              'k',
              'l',
              'm',
              'n',
              'o',
              'p',
            ],
    })
  }

  await client.redis.selva_index_new(
    '___selva_hierarchy',
    'descendants',
    '',
    'none',
    '',
    'root',
    '"g" "things" a'
  )
  await client.redis.selva_index_new(
    '___selva_hierarchy',
    'descendants',
    '',
    'none',
    '',
    'root',
    '"thing" f "abc" c'
  )
  await wait(1e3)
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
                $field: 'things',
                $operator: 'has',
                $value: 'g',
              },
            ],
          },
        },
      },
    })
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
  await wait(1e3)

  const istate = await getIndexingState(client)
  t.truthy(istate['root.J.ImciICJ0aGluZ3MiIGE='])
  t.truthy(istate['root.J.ImciICJ0aGluZ3MiIGE='].take_max_ave > 80, istate['root.J.ImciICJ0aGluZ3MiIGE='].take_max_ave)
  t.truthy(istate['root.J.ImciICJ0aGluZ3MiIGE='].tot_max_ave > 80, istate['root.J.ImciICJ0aGluZ3MiIGE='].tot_max_ave)
  t.truthy(istate['root.J.ImciICJ0aGluZ3MiIGE='].ind_take_max_ave > 10, istate['root.J.ImciICJ0aGluZ3MiIGE='].ind_take_max_ave)
  t.truthy(istate['root.J.ImciICJ0aGluZ3MiIGE='].card === '10', istate['root.J.ImciICJ0aGluZ3MiIGE='].card)
  t.truthy(istate['root.J.InRoaW5nIiBmICJhYmMiIGM='])
  t.truthy(istate['root.J.InRoaW5nIiBmICJhYmMiIGM='].take_max_ave > 80, istate['root.J.InRoaW5nIiBmICJhYmMiIGM='].take_max_ave)
  t.truthy(istate['root.J.InRoaW5nIiBmICJhYmMiIGM='].tot_max_ave > 80, istate['root.J.InRoaW5nIiBmICJhYmMiIGM='].tot_max_ave)
  t.truthy(istate['root.J.InRoaW5nIiBmICJhYmMiIGM='].ind_take_max_ave > 5, istate['root.J.InRoaW5nIiBmICJhYmMiIGM='].ind_take_max_ave)
  t.truthy(istate['root.J.InRoaW5nIiBmICJhYmMiIGM='].card === '1000', istate['root.J.InRoaW5nIiBmICJhYmMiIGM='].card)

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

  const q = async () =>
    await client.get({
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

  const ilist = await client.redis.selva_index_list('___selva_hierarchy')
  t.deepEqual(ilist[0], 'root.J.ImNhdCIgZyAjMyBG')
  t.truthy(ilist[1][0] > 10, `${ilist[1][0]}`)
  t.truthy(ilist[1][1] > 100, `${ilist[1][1]}`)
  t.truthy(ilist[1][2] > 10, `${ilist[1][2]}`)
  t.truthy(ilist[1][3] > 90, `${ilist[1][2]}`)

  await client.destroy()
})

test.serial('find index exists', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  for (let i = 0; i < 1000; i++) {
    const o = {
      type: 'league',
      name: `League ${i}`,
      thing: 'abc',
      things: ['a', 'b'],
    }
    if (i % 2) {
      delete o.thing
    } else {
      delete o.things
    }
    await client.set(o)
  }

  const q1 = async () =>
    client.get({
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
  const q2 = async () =>
    client.get({
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
  await wait(1e3)

  const ilist = await client.redis.selva_index_list('___selva_hierarchy')
  t.deepEqual(ilist[0], 'root.J.InRoaW5nIiBo')
  t.truthy(ilist[1][0] > 50, `${ilist[1][0]}`)
  t.truthy(ilist[1][1] > 100, `${ilist[1][1]}`)
  t.truthy(ilist[1][2] > 100, `${ilist[1][2]}`)
  t.truthy(ilist[1][3] > 490, `${ilist[1][2]}`)
  t.deepEqual(ilist[2], 'root.J.InRoaW5ncyIgaCBM', `${ilist[2]}`)
  t.truthy(ilist[3][0] > 70, `${ilist[3][0]}`)
  t.truthy(ilist[3][1] > 150, `${ilist[3][1]}`)
  t.truthy(ilist[3][2] > 120, `${ilist[3][2]}`)
  t.truthy(ilist[3][3] > 490, `${ilist[3][2]}`)

  await client.destroy()
})
