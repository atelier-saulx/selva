import test from 'ava'
import { connect } from '../client/src/index'
import { start } from '../server/src/index'
import './assertions'
import { wait } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6099
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
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          fun: { type: 'set', items: { type: 'string' } },
          location: { type: 'geo', search: true },
          related: { type: 'references', search: { type: ['TAG'] } },
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port: 6099 })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('find - geo', async t => {
  // simple nested - single query
  const client = connect({ port: 6099 }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    location: {
      lat: 60.12,
      lon: 120.67
    }
  })

  await client.set({
    type: 'match',
    name: 'match 2',
    location: {
      lat: 61.12,
      lon: 122.67
    }
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
                $field: 'location',
                $operator: 'distance',
                $value: [120, 60, 100, 'km']
              }
            ]
          }
        }
      }
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
                  $value: 'match'
                },
                {
                  $field: 'location',
                  $operator: 'distance',
                  $value: [120, 60, 1000, 'km']
                }
              ]
            }
          }
        }
      })
    ).items.map(x => x.name),
    ['match 1', 'match 2']
  )
})
