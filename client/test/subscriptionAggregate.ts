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

test.serial('simple count aggregate sub', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  t.plan(3)

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
  }

  await client.set({
    type: 'match',
    name: 'match 999',
  })

  const countObs = client.observe({
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
  })

  let i = 0
  countObs.subscribe((x) => {
    if (i === 0) {
      t.deepEqualIgnoreOrder(x, { id: 'root', matchCount: 4 })
    } else if (i === 1) {
      t.deepEqualIgnoreOrder(x, { id: 'root', matchCount: 5 })
    } else if (i === 2) {
      t.deepEqualIgnoreOrder(x, { id: 'root', matchCount: 8 })
    } else {
      t.fail()
    }
    i++
  })

  await wait(1e3)

  await client.set({
    $id: 'ma10',
    parents: ['le1'],
    type: 'match',
    name: 'match 10',
    value: 72,
  })

  await wait(1e3)

  await Promise.all([
    client.set({
      $id: 'ma11',
      parents: ['le2'],
      type: 'match',
      name: 'match 11',
      value: 73,
    }),
    client.set({
      $id: 'ma12',
      parents: ['le1'],
      type: 'match',
      name: 'match 12',
      value: 74,
    }),
    client.set({
      $id: 'ma13',
      parents: ['le2'],
      type: 'match',
      name: 'match 13',
      value: 75,
    }),
  ])

  await wait(2e3)

  await client.delete('root')
  await client.destroy()
})

test.serial('simple sum aggregate sub', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  t.plan(3)

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

  const countObs = client.observe({
    $id: 'root',
    id: true,
    thing: {
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
  })

  let i = 0
  countObs.subscribe((x) => {
    if (i === 0) {
      t.deepEqualIgnoreOrder(x, { id: 'root', thing: sum })
    } else if (i === 1) {
      t.deepEqualIgnoreOrder(x, { id: 'root', thing: sum + 72 })
    } else if (i === 2) {
      t.deepEqualIgnoreOrder(x, { id: 'root', thing: sum + 72 + 73 + 74 + 75 })
    } else {
      t.fail()
    }
    i++
  })

  await wait(1e3)

  await client.set({
    $id: 'ma10',
    parents: ['le1'],
    type: 'match',
    name: 'match 10',
    value: 72,
  })

  await wait(1e3)

  await Promise.all([
    client.set({
      $id: 'ma11',
      parents: ['le2'],
      type: 'match',
      name: 'match 11',
      value: 73,
    }),
    client.set({
      $id: 'ma12',
      parents: ['le1'],
      type: 'match',
      name: 'match 12',
      value: 74,
    }),
    client.set({
      $id: 'ma13',
      parents: ['le2'],
      type: 'match',
      name: 'match 13',
      value: 75,
    }),
  ])

  await wait(2e3)

  await client.delete('root')
  await client.destroy()
})

test.serial('list avg aggregate sub', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  t.plan(3)

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
  }

  await client.set({
    type: 'match',
    name: 'match 999',
  })

  const countObs = client.observe({
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
  })

  let i = 0
  countObs.subscribe((x) => {
    if (i === 0) {
      t.deepEqual(x, {
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
      })
    } else if (i === 1) {
      t.deepEqual(x, {
        id: 'root',
        leagues: [
          {
            name: 'league 0',
            valueAvg: (10 + 12) / 2,
          },
          {
            name: 'league 1',
            valueAvg: (11 + 13 + 72) / 3,
          },
        ],
      })
    } else if (i === 2) {
      t.deepEqual(x, {
        id: 'root',
        leagues: [
          {
            name: 'league 0',
            valueAvg: (10 + 12 + 73 + 75) / 4,
          },
          {
            name: 'league 1',
            valueAvg: (11 + 13 + 72 + 74) / 4,
          },
        ],
      })
    } else {
      t.fail()
    }
    i++
  })

  await wait(1e3)

  await client.set({
    $id: 'ma10',
    parents: ['le1'],
    type: 'match',
    name: 'match 10',
    value: 72,
  })

  await wait(1e3)

  await Promise.all([
    client.set({
      $id: 'ma11',
      parents: ['le0'],
      type: 'match',
      name: 'match 11',
      value: 73,
    }),
    client.set({
      $id: 'ma12',
      parents: ['le1'],
      type: 'match',
      name: 'match 12',
      value: 74,
    }),
    client.set({
      $id: 'ma13',
      parents: ['le0'],
      type: 'match',
      name: 'match 13',
      value: 75,
    }),
  ])

  await wait(2e3)

  await client.delete('root')
  await client.destroy()
})

test.serial('simple nested find avg aggregate sub', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  t.plan(3)

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

  const countObs = client.observe({
    $id: 'root',
    id: true,
    thing: {
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
  })

  let i = 0
  countObs.subscribe((x) => {
    if (i === 0) {
      t.deepEqualIgnoreOrder(x, { id: 'root', thing: sum / 4 })
    } else if (i === 1) {
      t.deepEqualIgnoreOrder(x, { id: 'root', thing: (sum + 72) / 5 })
    } else if (i === 2) {
      t.deepEqualIgnoreOrder(x, {
        id: 'root',
        thing: (sum + 72 + 73 + 74 + 75) / 8,
      })
    } else {
      t.fail()
    }
    i++
  })

  await wait(1e3)

  await client.set({
    $id: 'ma10',
    parents: ['le1'],
    type: 'match',
    name: 'match 10',
    value: 72,
  })

  await wait(1e3)

  await Promise.all([
    client.set({
      $id: 'ma11',
      parents: ['le2'],
      type: 'match',
      name: 'match 11',
      value: 73,
    }),
    client.set({
      $id: 'ma12',
      parents: ['le1'],
      type: 'match',
      name: 'match 12',
      value: 74,
    }),
    client.set({
      $id: 'ma13',
      parents: ['le2'],
      type: 'match',
      name: 'match 13',
      value: 75,
    }),
  ])

  await wait(2e3)

  await client.delete('root')
  await client.destroy()
})

test.serial('simple max aggregate sub', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  t.plan(3)

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
  }

  await client.set({
    type: 'match',
    name: 'match 999',
  })

  const countObs = client.observe({
    $id: 'root',
    id: true,
    val: {
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
  })

  let i = 0
  countObs.subscribe((x) => {
    if (i === 0) {
      t.deepEqualIgnoreOrder(x, { id: 'root', val: 13 })
    } else if (i === 1) {
      t.deepEqualIgnoreOrder(x, { id: 'root', val: 72 })
    } else if (i === 2) {
      t.deepEqualIgnoreOrder(x, { id: 'root', val: 75 })
    } else {
      t.fail()
    }
    i++
  })

  await wait(1e3)

  await client.set({
    $id: 'ma10',
    parents: ['le1'],
    type: 'match',
    name: 'match 10',
    value: 72,
  })

  await wait(1e3)

  await Promise.all([
    client.set({
      $id: 'ma11',
      parents: ['le2'],
      type: 'match',
      name: 'match 11',
      value: 73,
    }),
    client.set({
      $id: 'ma12',
      parents: ['le1'],
      type: 'match',
      name: 'match 12',
      value: 74,
    }),
    client.set({
      $id: 'ma13',
      parents: ['le2'],
      type: 'match',
      name: 'match 13',
      value: 75,
    }),
  ])

  await wait(2e3)

  await client.delete('root')
  await client.destroy()
})
