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
    selvaOptions: ['FIND_INDEXING_THRESHOLD', '5', 'FIND_INDICES_MAX', '2', 'FIND_INDEXING_INTERVAL', '10', 'FIND_INDEXING_ICB_UPDATE_INTERVAL', '1', 'FIND_INDEXING_POPULARITY_AVE_PERIOD', '1'],
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

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.afterEach(async (t) => {
  const client = connect({ port: port })
  await new Promise((r) => setTimeout(r, 100))
  await client.delete('root')
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: port })
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

const chars = '123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'

test.serial('find index', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.set({
    type: 'league',
    name: 'league 0',
  })
  for (let i = 0; i < chars.length; i++) {
    await client.set({
      type: 'league',
      name: `league ${i + 1}`,
      thing: `${chars.charAt(i)}`,
    })
  }

  const q = {
    $id: 'root',
    id: true,
    items: {
      name: true,
      $list: {
        $sort: { $field: 'thing', $order: 'asc' },
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

  for (let i = 0; i < 300; i++) {
    t.deepEqual(
      await client.get(q),
      {
        id: 'root',
        items: Array(chars.length).fill(null).map((_, i) => ({ name: `league ${i + 1}` }))
      }
    )
    await wait(1)
  }

  t.deepEqual((await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]), [
    'root.J.B.dGhpbmc=.ImxlIiBl',
    '36',
    'root.J.B.dGhpbmc=.InRoaW5nIiBo',
    '35',
  ])

  await client.destroy()
})
