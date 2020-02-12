import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { wait } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6090
  })

  await wait(500)

  const client = connect({ port: 6090 })
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
  const client = connect({ port: 6090 })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('find - references', async t => {
  // simple nested - single query
  const client = connect({ port: 6090 })
  const globMatches = []
  const leaguesSet = []
  for (let i = 0; i < 10; i++) {
    const matches = []
    for (let j = 0; j < 10; j++) {
      const match = {
        $id: await client.id({ type: 'match' }),
        type: 'match',
        name: 'match' + j,
        value: Number(i + '.' + j),
        related: globMatches.map(v => v.$id)
      }
      matches.push(match)
      globMatches.push(match)
    }
    leaguesSet.push({
      type: 'league',
      name: 'league' + i,
      value: i,
      children: matches
    })
  }
  await Promise.all(leaguesSet.map(v => client.set(v)))

  const { items: leagues } = await client.get({
    items: {
      id: true,
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'desc' },
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'league'
          }
        }
      }
    }
  })

  const league = leagues[0].id

  const { items: matches } = await client.get({
    $id: league,
    items: {
      id: true,
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'desc' },
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
              $operator: '..',
              $value: [5, 10]
            }
          ]
        }
      }
    }
  })

  const { items: relatedMatches } = await client.get({
    $id: matches[0].id,
    items: {
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'desc' },
        $find: {
          $traverse: 'related',
          $filter: [
            {
              $field: 'value',
              $operator: '<',
              $value: 4
            },
            {
              $field: 'value',
              $operator: '<',
              $value: 'now'
            },
            {
              $field: 'value',
              $operator: '>',
              $value: 2
            }
          ]
        }
      }
    }
  })

  t.deepEqual(relatedMatches, [
    { value: 4, name: 'match0' },
    { value: 3.9, name: 'match9' },
    { value: 3.8, name: 'match8' },
    { value: 3.7, name: 'match7' },
    { value: 3.6, name: 'match6' },
    { value: 3.5, name: 'match5' },
    { value: 3.4, name: 'match4' },
    { value: 3.3, name: 'match3' },
    { value: 3.2, name: 'match2' },
    { value: 3.1, name: 'match1' },
    { value: 3, name: 'match0' },
    { value: 2.9, name: 'match9' },
    { value: 2.8, name: 'match8' },
    { value: 2.7, name: 'match7' },
    { value: 2.6, name: 'match6' },
    { value: 2.5, name: 'match5' },
    { value: 2.4, name: 'match4' },
    { value: 2.3, name: 'match3' },
    { value: 2.2, name: 'match2' },
    { value: 2.1, name: 'match1' },
    { value: 2, name: 'match0' }
  ])

  const { items: relatedMatchesLeagues } = await client.get({
    $id: matches[0].id,
    items: {
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $find: {
          $traverse: 'related',
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'league'
              },
              {
                $field: 'value',
                $operator: '<',
                $value: 10
              }
            ]
          }
        }
      }
    }
  })

  t.deepEqualIgnoreOrder(
    relatedMatchesLeagues,
    [
      { value: 0, name: 'league0' },
      { value: 1, name: 'league1' },
      { value: 2, name: 'league2' },
      { value: 3, name: 'league3' },
      { value: 4, name: 'league4' },
      { value: 5, name: 'league5' },
      { value: 6, name: 'league6' },
      { value: 7, name: 'league7' },
      { value: 8, name: 'league8' },
      { value: 9, name: 'league9' }
    ],
    'Nested query'
  )

  await wait(1000)

  console.log('HERE HERE HERE')

  const { related: relatedMatchesLeaguesNoTraverse } = await client.get({
    $id: matches[0].id,
    related: {
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $find: {
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'league'
              },
              {
                $field: 'value',
                $operator: '<',
                $value: 10
              }
            ]
          }
        }
      }
    }
  })

  t.deepEqualIgnoreOrder(
    relatedMatchesLeaguesNoTraverse,
    [
      { value: 0, name: 'league0' },
      { value: 1, name: 'league1' },
      { value: 2, name: 'league2' },
      { value: 3, name: 'league3' },
      { value: 4, name: 'league4' },
      { value: 5, name: 'league5' },
      { value: 6, name: 'league6' },
      { value: 7, name: 'league7' },
      { value: 8, name: 'league8' },
      { value: 9, name: 'league9' }
    ],
    'Nested query'
  )

  await wait(1000)
})
