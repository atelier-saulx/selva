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
    selvaOptions: ['FIND_INDEXING_THRESHOLD', '2', 'FIND_INDICES_MAX', '1', 'FIND_INDEXING_INTERVAL', '10', 'FIND_INDEXING_ICB_UPDATE_INTERVAL', '1', 'FIND_INDEXING_POPULARITY_AVE_PERIOD', '1'],
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      thing: {
        prefix: 'th',
        fields: {
          name: { type: 'string' },
          subthings: { type: 'references' },
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

test.serial('find references', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  const mainThing = await client.set({
    type: 'thing',
    name: 'Main thing',
    subthings: [
      {
        type: 'thing',
        name: 'sub 1',
        subthings: [
          {
            type: 'thing',
            name: 'sub 2',
            subthings: [
              {
                type: 'thing',
                name: 'sub 3',
                subthings: [
                  {
                    type: 'thing',
                    name: 'sub 4',
                  },
                  {
                    type: 'thing',
                    name: 'sub 6',
                  },
                  {
                    type: 'thing',
                    name: 'sub 7',
                  },
                ]
              },
              {
                type: 'thing',
                name: 'sub 5',
              }
            ]
          },
          {
            type: 'thing',
            name: 'sub 8',
            subthings: [
              {
                type: 'thing',
                name: 'sub 10',
              }
            ]
          },
        ]
      },
      {
        type: 'thing',
        name: 'sub 9',
      },
    ]
  })

  const q = {
    $id: mainThing,
    items: {
      name: true,
      $list: {
        $find: {
          $traverse: 'subthings',
          $recursive: true,
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'thing',
            },
          ],
        },
      },
    },
  }

  console.log(await client.get(q))

  for (let i = 0; i < 300; i++) {
    t.deepEqualIgnoreOrder(
      await client.get(q),
      {
        items: [
          { name: 'sub 1' },
          { name: 'sub 2' },
          { name: 'sub 3' },
          { name: 'sub 4' },
          { name: 'sub 5' },
          { name: 'sub 6' },
          { name: 'sub 7' },
          { name: 'sub 8' },
          { name: 'sub 9' },
          { name: 'sub 10' },
        ]
      }
    )
    await wait(1)
  }

  console.log(await client.redis.selva_index_list('___selva_hierarchy'))
  t.deepEqual((await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]), [
    `${mainThing}.O.eyJzdWJ0aGluZ3MifQ==.InRoIiBl`,
    '10',
  ])

  await client.destroy()
})
