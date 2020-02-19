import test from 'ava'
import { connect } from '../client/src/index'
import { start } from '../server/src/index'
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
    title: {
      en: '🌊 mr flurpels 🌊'
    },
    description: {
      en: 'I like fancy 🌊'
    },
    children: [
      {
        type: 'team',
        name: 'team!',
        title: {
          en: '🌊 TEAM 🌊'
        },
        children: [
          {
            type: 'match',
            name: 'match time',
            start: Date.now() - 10000,
            end: Date.now() + 60 * 60 * 1000 * 2
          }
        ]
      }
    ]
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
          id: true,
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
          sport: { title: true, id: true, $inherit: { $item: 'sport' } },
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

  console.dir(result, { depth: 100 })

  t.true(true)
})
