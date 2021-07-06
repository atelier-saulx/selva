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

  await client.delete('root')
  await client.destroy()
})
