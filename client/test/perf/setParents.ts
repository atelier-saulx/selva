import test from 'ava'
import { performance } from 'perf_hooks'
import { connect } from '../../src/index'
import { start, startReplica } from '@saulx/selva-server'
import { wait, removeDump } from '../assertions'
import getPort from 'get-port'
import path from 'path'
let srv
let port: number

test.before(async (t) => {
  const r = removeDump(path.join(__dirname, '../../tmp'))

  r()
  port = await getPort()
  srv = await start({
    port,
    // selvaOptions: [
    //   'FIND_INDICES_MAX',
    //   '100',
    //   'FIND_INDEXING_INTERVAL',
    //   '1000',
    //   'FIND_INDEXING_ICB_UPDATE_INTERVAL',
    //   '500',
    //   'FIND_INDEXING_POPULARITY_AVE_PERIOD',
    //   '3',
    // ],
  })

  await startReplica({
    name: 'default',
    registry: { port },
    selvaOptions: [
      'FIND_INDICES_MAX',
      '100',
      'FIND_INDEXING_INTERVAL',
      '1000',
      'FIND_INDEXING_ICB_UPDATE_INTERVAL',
      '500',
      'FIND_INDEXING_POPULARITY_AVE_PERIOD',
      '3',
    ],
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      email: {
        fields: {
          name: { type: 'string' },
        },
      },
      user: {
        prefix: 'us',
        fields: {
          name: { type: 'string' },
          email: { type: 'string' },
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

test.serial('perf: find descendants', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await wait(2e3)

  const user = 'use8a03954'

  await client.set({
    $id: user,
    name: 'snurp',
  })

  client
    .observe({
      bla: {
        $list: {
          $offset: 0,
          // $function: 'count',
          // $sort: { $field: 'status', $order: 'desc' },
          $limit: 1000,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $field: 'type',
                $value: 'email',
              },
            ],
          },
        },
      },
    })
    .subscribe(() => {})

  for (let i = 0; i < 1000; i++) {
    const start = performance.now()
    const q: any[] = []
    for (let i = 0; i < 10e3; i++) {
      q.push(
        client.set({
          parents: [user],
          type: 'email',
        })
      )
    }
    await Promise.all(q)

    const time = performance.now() - start
    console.info('round:', i, time, 'ms')
  }

  t.pass('smurp')
})
