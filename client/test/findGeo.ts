import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'

let srv
test.before(async (t) => {
  srv = await start({
    port: 6099,
  })

  await wait(500)

  const client = connect({ port: 6099 }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          name: { type: 'string', search: { type: ['TAG'] } },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          fun: { type: 'set', items: { type: 'string' } },
          location: { type: 'geo', search: true },
          related: { type: 'references', search: { type: ['TAG'] } },
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: 6099 })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - geo', async (t) => {
  // simple nested - single query
  const client = connect({ port: 6099 }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    location: {
      lat: 60.12,
      lon: 120.67,
    },
  })

  await client.set({
    type: 'match',
    name: 'match 2',
    location: {
      lat: 61.12,
      lon: 122.67,
    },
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
                $field: 'location',
                $operator: 'distance',
                $value: {
                  $lon: 120,
                  $lat: 60,
                  $radius: 100000,
                },
              },
            ],
          },
        },
      },
    }),
    { id: 'root', items: [{ name: 'match 1' }] }
  )

  t.deepEqualIgnoreOrder(
    (
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
                  $field: 'location',
                  $operator: 'distance',
                  $value: {
                    $lon: 120,
                    $lat: 60,
                    $radius: 1000000,
                  },
                },
              ],
            },
          },
        },
      })
    ).items.map((x) => x.name),
    ['match 1', 'match 2']
  )

  await client.destroy()
})
