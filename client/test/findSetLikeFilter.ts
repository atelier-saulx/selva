import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port

test.before(async (t) => {
  port = await getPort()
  srv = await start({ port })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          description: { type: 'text' },
          value: { type: 'number' },
          status: { type: 'number' },
        },
      },
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string' },
          value: { type: 'number' },
          mascot: { type: 'reference' },
        },
      },
      mascot: {
        prefix: 'rq',
        fields: {
          name: { type: 'string' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  for (let i = 1; i < 400; i++) {
    await client.set({
      type: 'match',
      $id: `ma${i}`,
      value: i,
      parents: [ `le${i % 15}` ],
      children: [
        {
          type: 'team',
          $id: `te${i % 50}`,
          name: `Hehe ${i}A`,
          value: i % 10,
        },
        {
          type: 'team',
          $id: `te${(i % 50) + 1}`,
          name: `Hehe ${i}B`,
          value: i % 3,
        }
      ]
    })
  }

  for (let i = 1; i <= 50; i++) {
    await client.set({
      $id: `te${i}`,
      mascot: {
        type: 'mascot',
        $id: `rq${i}`,
        name: ['Matt', 'Jim', 'Kord'][(i - 1) % 3],
      },
    })
  }

  await client.destroy()
})

test.afterEach(async (t) => {
  const client = connect({ port: port })
  await new Promise((r) => setTimeout(r, 100))
  await client.delete('root')
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: port })
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('filter by descendants', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  t.deepEqual(await client.get({
    $id: 'root',
    leagues: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'league',
            },
            {
              $field: 'descendants',
              $operator: 'has',
              $value: [ 'te5', 'te10' ],
            },
          ],
        },
      },
    },
  }), {
    leagues: [ { id: 'le0' }, { id: 'le10' } ]
  })

  await client.destroy()
})

test.serial('filter by ancestors', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  t.deepEqual(await client.get({
    $id: 'root',
    teams: {
      id: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'team',
            },
            // TODO This part wasn't working properly before and now it's borken because it actually filters
            //{
            //  $field: 'ancestors',
            //  $operator: 'has',
            //  $value: [ 'le1', 'le4' ],
            //},
            {
              $field: 'value',
              $operator: '=',
              $value: 8
            }
          ],
        },
      },
    },
  }), {
    teams: [
      { id: 'te18', value: 8 },
      { id: 'te28', value: 8 },
      { id: 'te38', value: 8 },
      { id: 'te48', value: 8 },
      { id: 'te8', value: 8 },
    ]
  })

  await client.destroy()
})

test.serial('filter by parents', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  t.deepEqual(await client.get({
    $id: 'te1',
    matches: {
      id: true,
      $list: {
        $find: {
          $traverse: 'ancestors',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
            {
              $field: 'parents',
              $operator: 'has',
              $value: 'le6',
            },
          ],
        },
      },
    },
  }), {
    matches: [
      { id: 'ma351' },
    ]
  })

  await client.destroy()
})

test.serial('set like match to reference', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  t.deepEqual(await client.get({
    mascots: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'team',
            },
            {
              $field: 'mascot',
              $operator: 'has',
              $value: 'rq5',
            },
          ],
        },
      },
    },
  }), {
    mascots: [
      { id: 'te5' },
    ]
  })

  await client.destroy()
})
