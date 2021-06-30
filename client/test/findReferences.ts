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
          value: { type: 'number' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          fun: { type: 'set', items: { type: 'string' } },
          related: { type: 'references' },
          value: { type: 'number' },
          status: { type: 'number' },
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

test.serial('find - references', async (t) => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })
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
            $value: 'league',
          },
        },
      },
    },
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
              $value: 'match',
            },
            {
              $field: 'value',
              $operator: '..',
              $value: [5, 10],
            },
          ],
        },
      },
    },
  })

  const { items: relatedMatches } = await client.get({
    $id: matches[0].id,
    items: {
      name: true,
      special: { num: { $field: 'value' } },
      $list: {
        $sort: { $field: 'value', $order: 'desc' },
        $find: {
          $traverse: 'related',
          $filter: [
            {
              $field: 'value',
              $operator: '<',
              $value: 4,
            },
            {
              $field: 'value',
              $operator: '<',
              $value: 'now',
            },
            {
              $field: 'value',
              $operator: '>',
              $value: 2,
            },
          ],
        },
      },
    },
  })

  t.deepEqual(relatedMatches, [
    { special: { num: 4 }, name: 'match0' },
    { special: { num: 3.9 }, name: 'match9' },
    { special: { num: 3.8 }, name: 'match8' },
    { special: { num: 3.7 }, name: 'match7' },
    { special: { num: 3.6 }, name: 'match6' },
    { special: { num: 3.5 }, name: 'match5' },
    { special: { num: 3.4 }, name: 'match4' },
    { special: { num: 3.3 }, name: 'match3' },
    { special: { num: 3.2 }, name: 'match2' },
    { special: { num: 3.1 }, name: 'match1' },
    { special: { num: 3 }, name: 'match0' },
    { special: { num: 2.9 }, name: 'match9' },
    { special: { num: 2.8 }, name: 'match8' },
    { special: { num: 2.7 }, name: 'match7' },
    { special: { num: 2.6 }, name: 'match6' },
    { special: { num: 2.5 }, name: 'match5' },
    { special: { num: 2.4 }, name: 'match4' },
    { special: { num: 2.3 }, name: 'match3' },
    { special: { num: 2.2 }, name: 'match2' },
    { special: { num: 2.1 }, name: 'match1' },
    { special: { num: 2 }, name: 'match0' },
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
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'match',
          },
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'league',
              },
              {
                $field: 'value',
                $operator: '<',
                $value: 10,
              },
            ],
          },
        },
      },
    },
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
      { value: 9, name: 'league9' },
    ],
    'Nested query'
  )

  await wait(1000)

  const { related: relatedMatchesLeaguesNoTraverse } = await client.get({
    $id: matches[0].id,
    related: {
      name: true,
      special: { num: { $field: 'value' } },
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
                $value: 'league',
              },
              {
                $field: 'value',
                $operator: '<',
                $value: 10,
              },
            ],
          },
        },
      },
    },
  })

  t.deepEqualIgnoreOrder(
    relatedMatchesLeaguesNoTraverse,
    [
      // { value: 0, special: { num: 0 }, name: 'league0' },
      // { value: 1, special: { num: 1 }, name: 'league1' },
      // { value: 2, special: { num: 2 }, name: 'league2' },
      // { value: 3, special: { num: 3 }, name: 'league3' },
      // { value: 4, special: { num: 4 }, name: 'league4' },
      // { value: 5, special: { num: 5 }, name: 'league5' },
      // { value: 6, special: { num: 6 }, name: 'league6' },
      // { value: 7, special: { num: 7 }, name: 'league7' },
      // { value: 8, special: { num: 8 }, name: 'league8' },
      // { value: 9, special: { num: 9 }, name: 'league9' },
      { special: { num: 0 }, name: 'league0' },
      { special: { num: 1 }, name: 'league1' },
      { special: { num: 2 }, name: 'league2' },
      { special: { num: 3 }, name: 'league3' },
      { special: { num: 4 }, name: 'league4' },
      { special: { num: 5 }, name: 'league5' },
      { special: { num: 6 }, name: 'league6' },
      { special: { num: 7 }, name: 'league7' },
      { special: { num: 8 }, name: 'league8' },
      { special: { num: 9 }, name: 'league9' },
    ],
    'Nested query'
  )

  await wait(1000)

  await client.destroy()
})
