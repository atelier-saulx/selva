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
          name: { type: 'string', search: { type: ['TAG'] } },
          thing: { type: 'string', search: { type: ['EXISTS'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          value: {
            type: 'number',
            search: { type: ['NUMERIC', 'SORTABLE', 'EXISTS'] }
          },
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

test.serial('find - numeric exists field', async t => {
  // simple nested - single query
  const client = connect({ port: 6099 }, { loglevel: 'info' })
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
})

test.serial('find - string field only exists indexed', async t => {
  // simple nested - single query
  const client = connect({ port: 6099 }, { loglevel: 'info' })
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
})
