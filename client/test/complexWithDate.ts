import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: {
        title: { type: 'text' }
      }
    },
    types: {
      league: {
        prefix: 'le',
        fields: {
          title: {
            type: 'text',
            meta: { type: 'title' },
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          }
        }
      },
      match: {
        prefix: 'ma',
        hierarchy: {
          team: {
            excludeAncestryWith: ['league']
          }
        },
        fields: {
          title: {
            type: 'text',
            meta: { type: 'title' },
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          },
          date: {
            type: 'timestamp',
            search: { type: ['NUMERIC', 'SORTABLE'] }
          },
          published: {
            type: 'boolean',
            search: { type: ['TAG'] },
            meta: { type: 'enabled' }
          }
        }
      },
      team: {
        prefix: 'te',
        hierarchy: {
          team: {
            excludeAncestryWith: ['league']
          }
        },
        fields: {
          title: {
            type: 'text',
            meta: { type: 'title' },
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('yes', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    type: 'league',
    $id: 'le1',
    $language: 'en',
    title: 'League 1'
  })

  await client.set({
    type: 'team',
    $id: 'te1',
    $language: 'en',
    title: 'Team 1',
    parents: ['le1']
  })

  await client.set({
    type: 'match',
    $id: 'ma1',
    $language: 'en',
    title: 'Match 1',
    date: 1578039984000,
    published: true,
    parents: ['te1', 'root']
  })

  await wait(500)

  const result = await client.get({
    $language: 'en',
    matches: {
      id: true,
      title: true,
      date: true,
      teams: {
        id: true,
        title: true,
        $list: {
          $find: {
            $traverse: 'parents',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'team'
              }
            ]
          }
        }
      },
      team: {
        id: true,
        title: true,
        $find: {
          $traverse: 'ancestors',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'team'
            }
          ]
        }
      },
      $list: {
        $offset: 0,
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match'
            },
            {
              $field: 'published',
              $operator: '=',
              $value: true
            },
            {
              $field: 'date',
              $operator: '>',
              $value: 1577883600000
            },
            {
              $field: 'date',
              $operator: '<',
              $value: 1580515199000
            }
          ]
        }
      }
    }
  })

  console.log(JSON.stringify(result, null, 2))
  t.truthy(result.matches && result.matches.length)
  t.truthy(result.matches[0].teams && result.matches[0].teams.length)
  t.truthy(result.matches[0].team && result.matches[0].team.title)
})
