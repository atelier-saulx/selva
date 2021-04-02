import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(500)

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          fun: { type: 'set', items: { type: 'string' } },
          related: { type: 'references', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - sort by text', async (t) => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })
  const globMatches = []
  const leaguesSet = []
  for (let i = 0; i < 10; i++) {
    const matches = []
    for (let j = 0; j < 10; j++) {
      const match = {
        $id: await client.id({ type: 'match' }),
        $language: 'en',
        type: 'match',
        name: 'match' + j,
        title: 'match' + j,
        value: Number(i + '.' + j),
        related: globMatches.map((v) => v.$id),
      }
      matches.push(match)
      globMatches.push(match)
    }
    leaguesSet.push({
      type: 'league',
      name: 'league' + i,
      value: i,
      children: matches,
    })
  }
  await Promise.all(leaguesSet.map((v) => client.set(v)))

  const result = await client.get({
    $id: 'root',
    $language: 'en',
    children: {
      id: true,
      title: true,
      $list: {
        $sort: {
          $field: 'name',
          $order: 'asc',
        },
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'match',
          },
        },
      },
    },
  })

  console.log(result)

  for (let i = 0; i < result.children.length; i++) {
    const idx = Math.floor(i / 10)
    t.deepEqualIgnoreOrder(result.children[i].title, `match${idx}`)
  }

  const result2 = await client.get({
    $id: 'root',
    $language: 'en',
    children: {
      id: true,
      title: true,
      $list: {
        $sort: {
          $field: 'title',
          $order: 'asc',
        },
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'match',
          },
        },
      },
    },
  })

  console.log(result2)

  for (let i = 0; i < result2.children.length; i++) {
    const idx = Math.floor(i / 10)
    t.deepEqualIgnoreOrder(result2.children[i].title, `match${idx}`)
  }

  await client.destroy()
})
