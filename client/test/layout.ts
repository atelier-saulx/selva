import test from 'ava'
import { connect } from '../src/index'
import { start } from '@selva/server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          title: { type: 'text', search: true },
          description: { type: 'text', search: true }
        }
      },
      sport: {
        prefix: 'sp',
        fields: {
          title: { type: 'text', search: true },
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      club: {
        prefix: 'cl',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      team: {
        prefix: 'te',
        fields: {
          title: { type: 'text', search: true },
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text', search: true },
          end: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          start: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('layout query', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  // add theme and ads

  await client.set({
    $id: 'league1',
    $language: 'en',
    title: '🌊 mr flurpels 🌊',
    description: 'I like fancy 🌊',
    children: [
      {
        type: 'team',
        name: 'team!',
        title: '🌊 TEAM 🌊',
        children: [
          {
            type: 'match',
            name: 'match time',
            title: '🌊 MATCH 🌊',
            start: Date.now() - 10000,
            end: Date.now() + 60 * 60 * 1000 * 2
          }
        ]
      }
    ]
  })

  await client.set({
    $id: 'spfootball',
    $language: 'en',
    title: 'flurp football',
    children: ['league1']
  })

  const result = await client.get({
    $id: 'league1',
    id: true,
    $language: 'en',
    // theme: { $inherit: true },
    // ads: { $inherit: true },
    components: [
      {
        component: { $value: 'description' },
        title: {
          $field: 'title'
        },
        description: {
          $field: 'description'
        }
      },
      {
        component: { $value: 'gridLarge' },
        showall: { $value: true },
        children: {
          title: true,
          $list: {
            $range: [0, 100],
            $find: {
              $traverse: 'descendants',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'team'
                }
              ]
            }
          }
        }
      },
      {
        component: { $value: 'list' },
        children: {
          title: true,
          image: { icon: true, thumb: true },
          sport: { title: true, $inherit: { $item: 'sport' } },
          $list: {
            $sort: { $field: 'start', $order: 'asc' },
            $find: {
              $traverse: 'descendants',
              $filter: [
                {
                  $field: 'type',
                  $operator: '=',
                  $value: 'match'
                },
                {
                  $field: 'start',
                  $operator: '<',
                  $value: 'now'
                },
                {
                  $field: 'end',
                  $operator: '>',
                  $value: 'now'
                }
              ]
            }
          }
        }
      }
    ]
  })

  console.log(Date.now())
  console.dir(result, { depth: 100 })

  t.deepEqualIgnoreOrder(result, {
    id: 'league1',
    components: [
      {
        component: 'description',
        title: '🌊 mr flurpels 🌊',
        description: 'I like fancy 🌊'
      },
      {
        component: 'gridLarge',
        showall: true,
        children: [{ title: '🌊 TEAM 🌊' }]
      },
      {
        component: 'list',
        children: [{ sport: { title: 'flurp football' }, title: '🌊 MATCH 🌊' }]
      }
    ]
  })
})
