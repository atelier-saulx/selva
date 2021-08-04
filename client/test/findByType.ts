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
          matches: { type: 'references' },
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

test.serial('find - by type', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.set({
    $id: 'le1',
    type: 'league',
    name: 'league 1',
  })

  await client.set({
    $id: 'ma1',
    parents: ['le1'],
    type: 'match',
    name: 'match 1',
    value: 1,
  })

  await client.set({
    $id: 'ma2',
    parents: ['ma1'],
    type: 'match',
    name: 'match 2',
    value: 2,
  })

  await client.set({
    $id: 'ma3',
    parents: ['ma1'],
    type: 'match',
    name: 'match 3',
  })

  await client.set({
    $id: 'le1',
    matches: ['ma1', 'ma2', 'ma3'],
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        nonsense: { $default: 'yes' },
        $list: {
          $find: {
            $recursive: true,
            $traverse: {
              league: 'children',
              $any: false,
            },
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
      },
    }),
    {
      id: 'root',
      items: [
        { name: 'match 1', nonsense: 'yes' },
        // { name: 'match 2', nonsense: 'yes' },
      ],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        nonsense: { $default: 'yes' },
        $list: {
          $find: {
            $recursive: true,
            $traverse: {
              league: { $first: ['matches', 'children'] },
              $any: false,
            },
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
      },
    }),
    {
      id: 'root',
      items: [
        { name: 'match 1', nonsense: 'yes' },
        { name: 'match 2', nonsense: 'yes' },
      ],
    }
  )

  await client.delete('root')
  await client.destroy()
})
