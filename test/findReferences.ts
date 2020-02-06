import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { wait, dumpDb } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6090,
    developmentLogging: true,
    loglevel: 'info'
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
        value: j + i,
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

  const leagues = await client.query({
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
  })

  const league = leagues[0].id

  const matches = await client.query({
    $id: league,
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
  })

  const relatedMatches = await client.query({
    $id: matches[0].id,
    id: true,
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
            $value: 20
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
  })

  console.log('xxx', await client.get({ $id: matches[0].id, related: true }))

  // const m = (await dumpDb(client)).filter(v => {
  //   if (typeof v[1] === 'object') {
  //     return true
  //   }
  //   return false
  // })

  // console.log(m, m.length)

  console.log('RELATED', relatedMatches)
})
