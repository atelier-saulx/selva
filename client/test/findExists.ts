import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })

  await wait(500)
})

test.beforeEach(async t => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          thing: { type: 'string', search: { type: ['EXISTS'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          description: { type: 'text' },
          value: {
            type: 'number',
            search: { type: ['NUMERIC', 'SORTABLE', 'EXISTS'] }
          },
          status: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      }
    }
  })

  // A small delay is needed after setting the schema
  await new Promise(r => setTimeout(r, 100))

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port: port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - numeric exists field', async t => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    value: 1
  })

  await client.set({
    type: 'match',
    name: 'match 2'
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
                $value: 'match'
              },
              {
                $field: 'value',
                $operator: 'exists'
              }
            ]
          }
        }
      }
    }),
    { id: 'root', items: [{ name: 'match 1' }] }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('find - string field only exists indexed', async t => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    type: 'league',
    name: 'league 1'
  })

  await client.set({
    type: 'league',
    name: 'league 2',
    thing: 'yes some value here'
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
                $value: 'league'
              },
              {
                $field: 'thing',
                $operator: 'exists'
              }
            ]
          }
        }
      }
    }),
    { id: 'root', items: [{ name: 'league 2' }] }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('find - numeric not exists field', async t => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    value: 1
  })

  await client.set({
    type: 'match',
    name: 'match 2'
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
                $value: 'match'
              },
              {
                $field: 'value',
                $operator: 'notExists'
              }
            ]
          }
        }
      }
    }),
    { id: 'root', items: [{ name: 'match 2' }] }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('find - string field only not exists indexed', async t => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    type: 'league',
    name: 'league 1'
  })

  await client.set({
    type: 'league',
    name: 'league 2',
    thing: 'yes some value here'
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
              $value: 'league'
            },
            {
              $field: 'thing',
              $operator: 'notExists'
            }
          ]
        }
      }
    }
  })

  delete m.$meta

  t.deepEqualIgnoreOrder(m, { id: 'root', items: [{ name: 'league 1' }] })

  await client.delete('root')
  await client.destroy()
})

test.serial('find - text exists field', async t => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.set({
    $language: 'en',
    type: 'match',
    description: 'match 1',
    value: 1
  })

  await client.set({
    $language: 'en',
    type: 'match',
    name: 'match 2',
    value: 1
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
                $value: 'match'
              },
              {
                $field: 'description',
                $operator: 'exists'
              }
            ]
          }
        }
      }
    }),
    { id: 'root', items: [{ description: { en: 'match 1' } }] }
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
                $value: 'match'
              },
              {
                $field: 'description',
                $operator: 'exists'
              }
            ]
          }
        }
      }
    }),
    { id: 'root', items: [{ description: 'match 1' }] }
  )

  await client.delete('root')
  await client.destroy()
})
