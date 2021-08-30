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

  await wait(1e3)

  t.deepEqual(await client.redis.selva_index_list('___selva_hierarchy'), [
    'root.I.ImxlYWd1ZSIgZQ==',
    2,
  ])

  await client.delete('root')
  await client.destroy()
})
