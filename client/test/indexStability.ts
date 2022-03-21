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
  srv = await start({
    port,
    selvaOptions: ['FIND_INDICES_MAX', '2', 'FIND_INDEXING_INTERVAL', '1000', 'FIND_INDEXING_ICB_UPDATE_INTERVAL', '500', 'FIND_INDEXING_POPULARITY_AVE_PERIOD', '6'],
  })

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
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

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

test.serial('index stability', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  for (let i = 1; i < 400; i++) {
    await client.set({
      type: 'match',
      $id: `ma${i}`,
      value: i,
      parents: [ `le${i % 5}` ],
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

  const q1 = { // common
    $id: 'root',
    teams: {
      name: true,
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
              $field: 'value',
              $operator: '=',
              $value: 2,
            },
          ],
        },
      },
    },
  }
  const q2 = { // common
    $id: 'root',
    matches: {
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
              $value: 'match',
            }
          ],
        },
      },
    },
  }
  const q3 = { // semi rare
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
            {
              $field: 'value',
              $operator: '=',
              $value: 1,
            }
          ],
        },
      },
    },
  }
  const q4 = { // rare
    $id: 'root',
    leagues: {
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
              $value: 'league',
            }
          ],
        },
      },
    },
  }
  const q5 = { // rare
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
              $field: 'children',
              $operator: 'has',
              $value: [ 'ma1' ],
            },
          ],
        },
      },
    },
  }

  const N = 5 * 60
  const P = Array(N).fill(null).map((_, i) => async () => {
    await client.get(q1)
    await client.get(q2)

    if (i % 5 == 0) {
      await client.get(q3)
    }
    if (i % 60 == 0) {
      await client.get(q4)
      await client.get(q5)
    }

    if (i % 60 == 0) {
      const l = await client.redis.selva_index_list('___selva_hierarchy')
      const stateMap = {}

      for (let i = 0; i < l.length; i += 2) {
        const name = Buffer.from(l[i].split('.')[2], 'base64').toString()
        const state = l[i + 1][3]

        stateMap[name] = state;
      }

        if (i >= 120) {
            //console.log(stateMap)
            t.deepEqualIgnoreOrder(stateMap, {
              '"ma" e': '399',
              '"children" {"ma1"} l': 'not_active',
              '"le" e': 'not_active',
              '"te" e': '51',
              '"value" g #1 F': 'not_active',
              '"value" g #2 F': 'not_active'
            })
        }
    }
  })

  for (let i = 0; i < P.length; i++) {
    t.notThrows(P[i])
    await wait(100)
  }

  await client.destroy()
})
