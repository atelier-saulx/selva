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

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {}
      },
      team: {
        prefix: 'te',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
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

test.serial('get nested results', async t => {
  const client = connect({ port })

  const matches = []
  const teams = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team'
    })
  }

  for (let i = 0; i < 10; i++) {
    matches.push({
      name: 'match ' + i,
      type: 'match',
      value: i,
      parents: {
        $add: [
          teams[~~(Math.random() * teams.length)].$id,
          teams[~~(Math.random() * teams.length)].$id
        ]
      },
      status: i < 5 ? 100 : 300
    })
  }

  await Promise.all(teams.map(t => client.set(t)))

  await client.set({
    type: 'league',
    name: 'league 1',
    children: matches
  })

  const result = await client.get({
    $includeMeta: true,
    items: {
      name: true,
      id: true,
      teams: {
        id: true,
        name: true,
        flurpy: true,
        $list: {
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'team'
              },
              {
                $field: 'value',
                $operator: '!=',
                $value: 2
              }
            ]
          }
        }
      },
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match'
            }
          ]
        }
      }
    }
  })

  t.is(result.items.length, 10, 'items length')
  t.is(result.items[0].teams.length, 2, 'has teams')

  await wait(1e3)

  await client.delete('root')

  await wait(1e3)

  await client.destroy()

  t.true(true)
})

test.serial('get nested results with $all', async t => {
  const client = connect({ port })

  const matches = []
  const teams = []

  for (let i = 0; i < 100; i++) {
    teams.push({
      $id: await client.id({ type: 'team' }),
      name: 'team ' + i,
      type: 'team'
    })
  }

  for (let i = 0; i < 10; i++) {
    matches.push({
      name: 'match ' + i,
      type: 'match',
      value: i,
      parents: {
        $add: [
          teams[~~(Math.random() * teams.length)].$id,
          teams[~~(Math.random() * teams.length)].$id
        ]
      },
      status: i < 5 ? 100 : 300
    })
  }

  await Promise.all(teams.map(t => client.set(t)))

  await client.set({
    type: 'league',
    name: 'league 1',
    children: matches
  })

  const result = await client.get({
    items: {
      $all: true,
      teams: {
        $all: true,
        $list: {
          $find: {
            $traverse: 'ancestors',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'team'
              },
              {
                $field: 'value',
                $operator: '!=',
                $value: 2
              }
            ]
          }
        }
      },
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match'
            }
          ]
        }
      }
    }
  })

  t.is(result.items.length, 10, 'items length')
  t.is(result.items[0].teams.length, 2, 'has teams')

  await wait(1e3)
  await client.delete('root')
  await wait(1e3)

  await client.destroy()

  t.true(true)
})
