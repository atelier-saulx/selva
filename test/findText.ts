import test from 'ava'
import { connect } from '../client/src/index'
import { start } from '../server/src/index'
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

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          title: { type: 'text', search: true },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text', search: true },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('find - exact text match on exact field', async t => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    name: 'match 1',
    title: {
      en: 'a nice match'
    }
  })

  await client.set({
    type: 'match',
    name: 'match 2',
    title: {
      en: 'greatest match'
    }
  })

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
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
                  $field: 'title',
                  $operator: '=',
                  $value: 'greatest match'
                }
              ]
            }
          }
        }
      })
    ).items.map(x => x.name),
    ['match 2']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
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
                  $field: 'title',
                  $operator: '=',
                  $value: 'nice match'
                }
              ]
            }
          }
        }
      })
    ).items.map(x => x.name),
    ['match 1']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $id: 'root',
        $language: 'en',
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
                  $field: 'title',
                  $operator: '=',
                  $value: 'match'
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
