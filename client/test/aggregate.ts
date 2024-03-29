import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
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
          name: { type: 'string' },
          thing: { type: 'string' },
          matches: {
            type: 'references',
            bidirectional: { fromField: 'league' },
          },
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
          league: {
            type: 'reference',
            bidirectional: { fromField: 'matches' },
          },
        },
      },
    },
  })

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

test.serial('simple aggregate', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  let sum = 0

  await Promise.all([
    await client.set({
      $id: 'le0',
      name: `league 0`,
    }),
    await client.set({
      $id: 'le1',
      name: `league 1`,
    }),
  ])

  for (let i = 0; i < 4; i++) {
    await client.set({
      $id: 'ma' + i,
      parents: [`le${i % 2}`],
      type: 'match',
      name: `match ${i}`,
      value: i + 10,
    })

    sum += i + 10
  }

  await client.set({
    type: 'match',
    name: 'match 999',
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      matchCount: {
        $aggregate: {
          $function: 'count',
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
        },
      },
    }),
    { id: 'root', matchCount: 4 }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      valueSum: {
        $aggregate: {
          $function: { $name: 'sum', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
        },
      },
    }),
    { id: 'root', valueSum: sum }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      valueAvg: {
        $aggregate: {
          $function: { $name: 'avg', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
        },
      },
    }),
    { id: 'root', valueAvg: sum / 4 }
  )

  t.deepEqual(
    await client.get({
      $id: 'root',
      id: true,
      leagues: {
        name: true,
        valueAvg: {
          $aggregate: {
            $function: { $name: 'avg', $args: ['value'] },
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'value',
                $operator: 'exists',
              },
            ],
          },
        },
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'league',
              },
            ],
          },
        },
      },
    }),
    {
      id: 'root',
      leagues: [
        {
          name: 'league 0',
          valueAvg: (10 + 12) / 2,
        },
        {
          name: 'league 1',
          valueAvg: (11 + 13) / 2,
        },
      ],
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'root',
      id: true,
      valueAvg: {
        $aggregate: {
          $function: { $name: 'avg', $args: ['value'] },
          $traverse: ['ma1', 'ma2', 'ma3'],
        },
      },
    }),
    {
      id: 'root',
      valueAvg: (11 + 12 + 13) / 3,
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      valueAvg: {
        $aggregate: {
          $function: { $name: 'avg', $args: ['value'] },
          $traverse: 'children',
          $find: {
            $traverse: 'children',
          },
        },
      },
    }),
    {
      id: 'root',
      valueAvg: sum / 4,
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      valueAvg: {
        $aggregate: {
          $function: { $name: 'avg', $args: ['value'] },
          $traverse: 'children',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'league',
            },
          ],
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
      valueAvg: sum / 4,
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      value: {
        $aggregate: {
          $function: { $name: 'min', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
        },
      },
    }),
    { id: 'root', value: 10 }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      value: {
        $aggregate: {
          $function: { $name: 'max', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
        },
      },
    }),
    { id: 'root', value: 13 }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      value: {
        $aggregate: {
          $function: { $name: 'countUnique', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
        },
      },
    }),
    { id: 'root', value: 4 }
  )
  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      value: {
        $aggregate: {
          $function: { $name: 'countUnique', $args: ['type'] },
          $traverse: 'descendants',
        },
      },
    }),
    { id: 'root', value: 2 }
  )

  let err = await t.throwsAsync(
    client.get({
      $id: 'root',
      id: true,
      value: {
        $aggregate: {
          $function: { $name: 'max', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type$',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
        },
      },
    })
  )
  t.assert(err.stack.includes('contains unsupported characters'))

  err = await t.throwsAsync(
    client.get({
      $id: 'root',
      id: true,
      value: {
        $aggregate: {
          $function: { $name: 'max', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: '_value',
              $operator: 'exists',
            },
          ],
        },
      },
    })
  )
  t.assert(err.stack.includes('contains unsupported characters'))

  await client.delete('root')
  await client.destroy()
})

test.serial('simple aggregate with reference fields', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })
  let sum = 0

  await Promise.all([
    await client.set({
      $id: 'le0',
      name: `league 0`,
    }),
    await client.set({
      $id: 'le1',
      name: `league 1`,
    }),
  ])

  for (let i = 0; i < 4; i++) {
    await client.set({
      $id: 'ma' + i,
      league: `le${i % 2}`,
      type: 'match',
      name: `match ${i}`,
      value: i + 10,
    })

    sum += (i % 2) * (i + 10)
  }

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'le1',
      id: true,
      valueSum: {
        $aggregate: {
          $function: { $name: 'sum', $args: ['value'] },
          $traverse: 'matches',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
        },
      },
    }),
    { id: 'le1', valueSum: sum }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('sorted aggregate', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  await Promise.all([
    await client.set({
      $id: 'le0',
      name: `league 0`,
    }),
    await client.set({
      $id: 'le1',
      name: `league 1`,
    }),
  ])

  for (let i = 0; i < 40; i++) {
    await client.set({
      $id: 'ma' + i,
      parents: [`le${i % 2}`],
      type: 'match',
      name: `match ${i}`,
      value: i + 10,
    })
  }

  await client.set({
    type: 'match',
    name: 'match 999',
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      matchCount: {
        $aggregate: {
          $function: 'count',
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
          $sort: {
            $order: 'asc',
            $field: 'value',
          },
          $limit: 4,
        },
      },
    }),
    { id: 'root', matchCount: 4 }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      valueSum: {
        $aggregate: {
          $function: { $name: 'sum', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
          $sort: {
            $order: 'asc',
            $field: 'value',
          },
          $limit: 4,
        },
      },
    }),
    { id: 'root', valueSum: 46 }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      valueAvg: {
        $aggregate: {
          $function: { $name: 'avg', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
          $sort: {
            $order: 'asc',
            $field: 'value',
          },
          $limit: 4,
        },
      },
    }),
    { id: 'root', valueAvg: 46 / 4 }
  )

  t.deepEqual(
    await client.get({
      $id: 'root',
      id: true,
      valueAvg: {
        $aggregate: {
          $function: { $name: 'avg', $args: ['value'] },
          $traverse: ['ma1', 'ma2', 'ma3'],
          $sort: {
            $order: 'asc',
            $field: 'value',
          },
          $limit: 4,
        },
      },
    }),
    {
      id: 'root',
      valueAvg: (11 + 12 + 13) / 3,
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      valueAvg: {
        $aggregate: {
          $function: { $name: 'avg', $args: ['value'] },
          $traverse: 'children',
          $find: {
            $traverse: 'children',
          },
          $sort: {
            $order: 'asc',
            $field: 'value',
          },
          $limit: 4,
        },
      },
    }),
    {
      id: 'root',
      valueAvg: 46 / 4,
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      valueAvg: {
        $aggregate: {
          $function: { $name: 'avg', $args: ['value'] },
          $traverse: 'children',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'league',
            },
          ],
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
          $sort: {
            $order: 'desc',
            $field: 'value',
          },
          $limit: 4,
        },
      },
    }),
    {
      id: 'root',
      valueAvg: 190 / 4,
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      value: {
        $aggregate: {
          $function: { $name: 'min', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
          $sort: {
            $order: 'asc',
            $field: 'value',
          },
          $limit: 4,
        },
      },
    }),
    { id: 'root', value: 10 }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      value: {
        $aggregate: {
          $function: { $name: 'max', $args: ['value'] },
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: 'exists',
            },
          ],
          $sort: {
            $order: 'asc',
            $field: 'value',
          },
          $limit: 4,
        },
      },
    }),
    { id: 'root', value: 13 }
  )

  await client.delete('root')
  await client.destroy()
})
