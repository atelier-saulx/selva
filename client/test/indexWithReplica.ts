import test from 'ava'
import { connect } from '../src/index'
import {
  startSubscriptionManager,
  startSubscriptionRegistry,
  startReplica,
  startOrigin,
  startRegistry,
  SelvaServer,
} from '@saulx/selva-server'
import getPort from 'get-port'
import './assertions'
import { wait, removeDump, getIndexingState } from './assertions'
import { join } from 'path'
const dir = join(process.cwd(), 'tmp', 'index-with-replica-test')

let port: number
let servers: Array<SelvaServer>

test.before(async (t) => {
  removeDump(dir)

  port = await getPort()
  servers = await Promise.all([
    startRegistry({ port }),
    startOrigin({
      dir,
      registry: { port },
      default: true,
      selvaOptions: [
        'FIND_INDICES_MAX',
        '0',
      ],
    }),
    startSubscriptionManager({ registry: { port } }),
    startSubscriptionRegistry({ registry: { port } }),
    startReplica({
      dir: join(dir, 'replica'),
      registry: { port },
      default: true,
      selvaOptions: [
        'FIND_INDICES_MAX',
        '10',
        'FIND_INDEXING_INTERVAL',
        '1000',
        'FIND_INDEXING_ICB_UPDATE_INTERVAL',
        '500',
        'FIND_INDEXING_POPULARITY_AVE_PERIOD',
        '3',
      ],
    }),
  ])

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
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
})

test.after(async (t) => {
  await Promise.all(servers.map((s) => s.destroy()))
  await t.connectionsAreEmpty()
  removeDump(dir)()
})

test.serial('Indexing with a replica', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  //console.log(await client.redis.info())
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

  const q1 = {
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

  for (let i = 0; i < 5e3; i++) {
    await client.get(q1)
    await wait(2)
  }

  const stateMap = await getIndexingState(client)
  t.deepEqual(stateMap['root.J.InRlIiBl']?.card, '51')
  t.deepEqual(stateMap['root.J.InZhbHVlIiBnICMyIEY=']?.card, '6')

  await client.destroy()
})
