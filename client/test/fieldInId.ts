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
    languages: ['en', 'de'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text',
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          },
          published: { type: 'boolean', search: { type: ['TAG'] } },
          awayTeam: { type: 'reference' },
          homeTeam: { type: 'reference' }
        }
      },
      team: {
        prefix: 'te',
        fields: {
          title: {
            type: 'text',
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          }
        }
      }
    }
  })
  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('$field in $id should work', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $language: 'en',
    $id: 'match1',
    title: 'matcho uno',
    published: true,
    awayTeam: 'team3',
    homeTeam: 'team4',
    children: [
      {
        $id: 'team3',
        title: 'team three'
      },
      {
        $id: 'team4',
        title: 'team four'
      }
    ]
  })

  await client.set({
    $language: 'en',
    $id: 'match2',
    title: 'matcho due',
    published: true,
    awayTeam: 'team1',
    homeTeam: 'team2',
    children: [
      {
        $id: 'team1',
        title: 'team one'
      },
      {
        $id: 'team2',
        title: 'team two'
      }
    ]
  })

  const result = await client.get({
    $language: 'en',
    children: {
      homeTeam: true,
      awayTeam: true,
      teams: [
        {
          id: true,
          $id: {
            $field: 'homeTeam'
          }
        },
        {
          id: true,
          $id: {
            $field: 'awayTeam'
          }
        }
      ],
      id: true,
      $list: {
        $limit: 20,
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
            }
          ]
        }
      }
    }
  })

  t.deepEqualIgnoreOrder(result, {
    children: [
      {
        id: 'match2',
        awayTeam: 'team1',
        teams: [
          {
            id: 'team2'
          },
          {
            id: 'team1'
          }
        ],
        homeTeam: 'team2'
      },
      {
        id: 'match1',
        awayTeam: 'team3',
        teams: [
          {
            id: 'team3'
          },
          {
            id: 'team4'
          }
        ],
        homeTeam: 'team4'
      }
    ]
  })

  await client.delete('root')
  await client.destroy()
})
