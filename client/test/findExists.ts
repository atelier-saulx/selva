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
      special: {
        prefix: 'sp',
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

test.serial('find - numeric exists field', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    value: 1,
  })

  await client.set({
    type: 'match',
    name: 'match 2',
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
      },
    }),
    { id: 'root', items: [{ name: 'match 1', nonsense: 'yes' }] }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('find - string field only exists', async (t) => {
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

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'children',
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

  await client.delete('root')
  await client.destroy()
})

test.serial('find - numeric not exists field', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    value: 1,
  })

  await client.set({
    type: 'match',
    name: 'match 2',
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'value',
                $operator: 'notExists',
              },
            ],
          },
        },
      },
    }),
    { id: 'root', items: [{ name: 'match 2' }] }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('find - string field only not exists indexed', async (t) => {
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

  const m = await client.get({
    $includeMeta: true,
    $id: 'root',
    id: true,
    items: {
      name: true,
      $list: {
        $find: {
          $traverse: 'children',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'league',
            },
            {
              $field: 'thing',
              $operator: 'notExists',
            },
          ],
        },
      },
    },
  })

  delete m.$meta

  t.deepEqualIgnoreOrder(m, { id: 'root', items: [{ name: 'league 1' }] })

  await client.delete('root')
  await client.destroy()
})

test.serial('find - text exists field', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    $language: 'en',
    $id: 'ma1',
    type: 'match',
    description: 'match 1',
    value: 1,
  })

  await client.set({
    $language: 'en',
    $id: 'ma2',
    type: 'match',
    name: 'match 2',
    value: 1,
  })

  await client.set({
    $language: 'en',
    $id: 'le1',
    type: 'league',
    name: 'league 1',
  })

  await client.set({
    $language: 'en',
    $id: 'sp1',
    type: 'special',
    name: 'special 1',
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        description: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'description',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    }),
    { id: 'root', items: [{ description: { en: 'match 1' } }] }
  )

  // TODO: make a separate test case from this
  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        id: true,
        items: {
          id: true,
          $fieldsByType: {
            match: { description: true, name: true },
            league: {
              id: true,
              name: true,
            },
          },
          $list: {
            $find: {
              $traverse: 'children',
            },
          },
        },
      })
    ).items,
    [
      { id: 'ma1', description: { en: 'match 1' } },
      { id: 'ma2', name: 'match 2' },
      { id: 'le1', name: 'league 1' },
      { id: 'sp1' },
    ]
  )
  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        id: true,
        items: {
          id: true,
          $fieldsByType: {
            match: { description: true, name: true },
            league: {
              id: true,
              name: true,
              parents: {
                id: true,
                $list: {
                  $find: {
                    $traverse: 'parents',
                  },
                },
              },
            },
          },
          $list: {
            $find: {
              $traverse: 'children',
            },
          },
        },
      })
    ).items,
    [
      { id: 'ma1', description: { en: 'match 1' } },
      { id: 'ma2', name: 'match 2' },
      { id: 'le1', name: 'league 1', parents: [{ id: 'root' }] },
      { id: 'sp1' },
    ]
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $id: 'root',
      id: true,
      items: {
        description: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'description',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    }),
    { id: 'root', items: [{ description: 'match 1' }] }
  )

  let err = await t.throwsAsync(
    client.get({
      $language: 'en',
      $id: 'root',
      id: true,
      items: {
        description: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type$',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'description',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    })
  )
  t.assert(err.stack.includes('contains unsupported characters'))

  err = await t.throwsAsync(
    client.get({
      $language: 'en',
      $id: 'root',
      id: true,
      items: {
        description: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: '_description',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    })
  )
  t.assert(err.stack.includes('contains unsupported characters'))

  await client.delete('root')
  await client.destroy()
})
