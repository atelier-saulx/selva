import test from 'ava'
import { connect } from '../client/src/index'
import { start } from '../server/src/index'
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
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          startTime: {
            type: 'timestamp',
            search: { type: ['NUMERIC', 'SORTABLE'] }
          },
          endTime: {
            type: 'timestamp',
            search: { type: ['NUMERIC', 'SORTABLE'] }
          },
          fun: { type: 'set', items: { type: 'string' } },
          related: { type: 'references', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC'] } }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial.skip('find - live', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    type: 'match',
    name: 'match 1',
    startTime: Date.now() - 5 * 60 * 1000, // 5 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'match 2',
    startTime: Date.now() - 2 * 60 * 1000, // 2 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'match 2',
    startTime: Date.now() - 2 * 60 * 60 * 1000, // 2 horus ago
    endTime: Date.now() - 60 * 60 * 1000 // ended 1 hour ago
  })

  console.log(await client.redis.hgetall(match1))

  console.log(
    await client.get({
      $id: 'root',
      children: {
        name: true,
        startTime: true,
        endTime: true,
        $list: {}
      }
    })
  )

  console.log(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'desc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'endTime',
                  $operator: '>',
                  $value: 'now'
                }
              ]
            }
          }
        }
      })
    ).$meta.query
  )
})

test.serial('find - already started', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    type: 'match',
    name: 'started 5m ago',
    startTime: Date.now() - 5 * 60 * 1000, // 5 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'started 2m ago',
    startTime: Date.now() - 2 * 60 * 1000, // 2 minutes ago
    endTime: Date.now() + 60 * 60 * 1000 // ends in 1 hour
  })

  await client.set({
    type: 'match',
    name: 'started 2h ago',
    startTime: Date.now() - 2 * 60 * 60 * 1000, // 2 hours ago
    endTime: Date.now() - 60 * 60 * 1000 // ended 1 hour ago
  })

  await client.set({
    $id: 'maFuture',
    type: 'match',
    name: 'starts in 1h',
    startTime: Date.now() + 1 * 60 * 60 * 1000, // starts in 1 hour
    endTime: Date.now() + 2 * 60 * 60 * 1000 // ends in 2 hours
  })

  await client.set({
    $id: 'maLaterFuture',
    type: 'match',
    name: 'starts in 2h',
    startTime: Date.now() + 2 * 60 * 60 * 1000, // starts in 1 hour
    endTime: Date.now() + 3 * 60 * 60 * 1000 // ends in 2 hours
  })

  console.log(await client.redis.hgetall(match1))

  console.log(
    await client.get({
      $id: 'root',
      children: {
        name: true,
        startTime: true,
        endTime: true,
        $list: {}
      }
    })
  )

  console.log(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'desc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'startTime',
                  $operator: '<',
                  $value: 'now'
                }
              ]
            }
          }
        }
      })
    ).$meta.query
  )
})
