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
  await wait(500)

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('find - single', async t => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })

  const team = await client.set({
    $id: 'te0',
    type: 'team',
    name: 'team0'
  })
  const matches = []
  for (let i = 0; i < 11; i++) {
    matches.push({
      $id: await client.id({ type: 'match' }),
      type: 'match',
      name: 'match' + i,
      parents: [team]
    })
  }

  await Promise.all(matches.map(v => client.set(v)))

  const r = await client.get({
    $id: 'te0',
    singleMatch: {
      name: true,
      $find: {
        $traverse: 'children',
        $filter: [
          {
            $field: 'type',
            $operator: '=',
            $value: 'match'
          },
          {
            $field: 'name',
            $operator: '=',
            $value: 'match0'
          }
        ]
      }
    }
  })

  t.deepEqual(r, {
    singleMatch: { name: 'match0' }
  })
})

test.serial('find - single with no wrapping', async t => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })

  const team = await client.set({
    $id: 'te0',
    type: 'team',
    name: 'team0'
  })
  const matches = []
  for (let i = 0; i < 11; i++) {
    matches.push({
      $id: await client.id({ type: 'match' }),
      type: 'match',
      name: 'match' + i,
      parents: [team]
    })
  }

  await Promise.all(matches.map(v => client.set(v)))

  const r = await client.get({
    $id: 'te0',
    name: true,
    $find: {
      $traverse: 'children',
      $filter: [
        {
          $field: 'type',
          $operator: '=',
          $value: 'match'
        },
        {
          $field: 'name',
          $operator: '=',
          $value: 'match0'
        }
      ]
    }
  })

  t.deepEqual(r, {
    name: 'match0'
  })
})

test.serial('find - single in array', async t => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })

  const team = await client.set({
    $id: 'te0',
    type: 'team',
    name: 'team0'
  })
  const matches = []
  for (let i = 0; i < 11; i++) {
    matches.push({
      $id: await client.id({ type: 'match' }),
      type: 'match',
      name: 'match' + i,
      parents: [team]
    })
  }

  await Promise.all(matches.map(v => client.set(v)))

  const r = await client.get({
    $id: 'te0',
    results: [
      {
        singleMatch: {
          name: true,
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match'
              },
              {
                $field: 'name',
                $operator: '=',
                $value: 'match0'
              }
            ]
          }
        }
      }
    ]
  })

  t.deepEqual(r, {
    results: [{ singleMatch: { name: 'match0' } }]
  })
})

test.serial('find - single no wrapping in array', async t => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })

  const team = await client.set({
    $id: 'te0',
    type: 'team',
    name: 'team0'
  })
  const matches = []
  for (let i = 0; i < 11; i++) {
    matches.push({
      $id: await client.id({ type: 'match' }),
      type: 'match',
      name: 'match' + i,
      parents: [team]
    })
  }

  await Promise.all(matches.map(v => client.set(v)))

  const r = await client.get({
    $id: 'te0',
    results: [
      {
        name: true,
        $find: {
          $traverse: 'children',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match'
            },
            {
              $field: 'name',
              $operator: '=',
              $value: 'match0'
            }
          ]
        }
      }
    ]
  })

  t.deepEqual(r, {
    results: [{ name: 'match0' }]
  })
})
