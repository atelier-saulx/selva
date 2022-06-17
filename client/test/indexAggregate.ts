import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { getIndexingState, wait } from './assertions'
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

test.serial('simple aggregate with indexing', async (t) => {
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

  for (let i = 0; i < 4000; i++) {
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

  for (let i = 0; i < 30; i++) {
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
      { id: 'root', valueAvg: sum / 4000 }
    )

    await wait(300)
  }

  const indState = await getIndexingState(client);
  t.deepEqualIgnoreOrder(indState['root.J.Im1hIiBl'].card, '4001')
  t.deepEqualIgnoreOrder(indState['root.J.InZhbHVlIiBo'].card, '4000')
  t.truthy(Number(indState['root.J.InZhbHVlIiBo'].ind_take_max_ave) > 3000)

  await client.delete('root')
  await client.destroy()
})
