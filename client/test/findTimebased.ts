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
  await client.destroy()
  await srv.destroy()
})

test.serial('subs upcoming, live and past', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const now = Date.now()
  let result

  await client.set({
    type: 'match',
    $id: 'ma1',
    name: 'upcoming match',
    startTime: now + 2000, // 2 sec from now
    endTime: now + 5000 // 5 sec from now
  })

  client
    .observe({
      past: {
        id: true,
        $list: {
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $value: 'now',
                $field: 'endTime',
                $operator: '<'
              }
            ]
          }
        }
      },
      live: {
        id: true,
        $list: {
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $value: 'now',
                $field: 'startTime',
                $operator: '<'
              },
              {
                $value: 'now',
                $field: 'endTime',
                $operator: '>'
              }
            ]
          }
        }
      },
      upcoming: {
        id: true,
        $list: {
          $limit: 10,
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $operator: '=',
                $value: 'match',
                $field: 'type'
              },
              {
                $value: 'now',
                $field: 'startTime',
                $operator: '>'
              }
            ]
          }
        }
      }
    })
    .subscribe(r => {
      result = r
    })

  await wait(500)
  t.deepEqualIgnoreOrder(result, {
    upcoming: [{ id: 'ma1' }],
    past: [],
    live: []
  })
  await wait(3000)
  t.deepEqualIgnoreOrder(result, {
    upcoming: [],
    past: [],
    live: [{ id: 'ma1' }]
  })
  await wait(3000)
  t.deepEqualIgnoreOrder(result, {
    upcoming: [],
    past: [{ id: 'ma1' }],
    live: []
  })

  await client.delete('root')
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

  const nextRefresh = Date.now() + 1 * 60 * 60 * 1000
  await client.set({
    $id: 'maFuture',
    type: 'match',
    name: 'starts in 1h',
    startTime: nextRefresh, // starts in 1 hour
    endTime: Date.now() + 2 * 60 * 60 * 1000 // ends in 2 hours
  })

  await client.set({
    $id: 'maLaterFuture',
    type: 'match',
    name: 'starts in 2h',
    startTime: Date.now() + 2 * 60 * 60 * 1000, // starts in 1 hour
    endTime: Date.now() + 3 * 60 * 60 * 1000 // ends in 2 hours
  })

  t.deepEqualIgnoreOrder(
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
    ).$meta.___refreshAt,
    nextRefresh
  )

  // FIXME: wft ASC sort broken?
  // t.deepEqual(
  //   (
  //     await client.get({
  //       $includeMeta: true,
  //       $id: 'root',
  //       items: {
  //         name: true,
  //         value: true,
  //         $list: {
  //           $sort: { $field: 'startTime', $order: 'asc' },
  //           $find: {
  //             $traverse: 'children',
  //             $filter: [
  //               {
  //                 $field: 'startTime',
  //                 $operator: '<',
  //                 $value: 'now'
  //               }
  //             ]
  //           }
  //         }
  //       }
  //     })
  //   ).items.map(i => i.name),
  //   ['started 2m ago', 'started 5m ago', 'started 2h ago']
  // )

  await client.delete('root')
  await client.destroy()
})

test.serial('find - already started subscription', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
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

  const nextRefresh = Date.now() + 5 * 1000
  const nextNextRefresh = Date.now() + 7 * 1000

  // add another <============== THIS BREAKS IT
  client
    .observe({
      $id: 'rando',
      items: {
        name: true,
        value: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'endTime',
                $operator: '<',
                $value: 'now'
              }
            ]
          }
        }
      }
    })
    .subscribe(() => {})
  // =======================

  await client.set({
    $id: 'maFuture',
    type: 'match',
    name: 'starts in 5s',
    startTime: nextRefresh,
    endTime: Date.now() + 2 * 60 * 60 * 1000 // ends in 2 hours
  })

  await client.set({
    $id: 'maLaterFuture',
    type: 'match',
    name: 'starts in 7s',
    startTime: nextNextRefresh,
    endTime: Date.now() + 3 * 60 * 60 * 1000 // ends in 2 hours
  })

  t.plan(5)

  const observable = client.observe({
    $includeMeta: true,
    $id: 'root',
    items: {
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'startTime', $order: 'asc' },
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

  let o1counter = 0
  const sub = observable.subscribe(d => {
    if (o1counter === 0) {
      // gets start event
      t.true(d.items.length === 3)
    } else if (o1counter === 1) {
      // gets update event
      t.true(d.items.length === 4)
      t.true(d.items.map(i => i.name).includes('starts in 5s'))
    } else if (o1counter === 2) {
      t.true(d.items.length === 5)
      t.true(d.items.map(i => i.name).includes('starts in 7s'))
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  await wait(10 * 1000)

  sub.unsubscribe()
  await client.delete('root')
})

test.serial('find - starting soon', async t => {
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

  const nextRefresh = Date.now() + 1 * 60 * 60 * 1000
  await client.set({
    $id: 'maFuture',
    type: 'match',
    name: 'starts in 1h',
    startTime: nextRefresh, // starts in 1 hour
    endTime: Date.now() + 2 * 60 * 60 * 1000 // ends in 2 hours
  })

  await client.set({
    $id: 'maLaterFuture',
    type: 'match',
    name: 'starts in 2h',
    startTime: Date.now() + 2 * 60 * 60 * 1000, // starts in 2 hour
    endTime: Date.now() + 3 * 60 * 60 * 1000 // ends in 3 hours
  })

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'asc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'startTime',
                  $operator: '>',
                  $value: 'now+1h'
                },
                {
                  $field: 'startTime',
                  $operator: '<',
                  $value: 'now+3h'
                }
              ]
            }
          }
        }
      })
    ).items.map(i => i.name),
    ['starts in 2h']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'asc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'startTime',
                  $operator: '..',
                  $value: ['now+1h', 'now+3h']
                }
              ]
            }
          }
        }
      })
    ).items.map(i => i.name),
    ['starts in 2h']
  )

  t.deepEqualIgnoreOrder(
    (
      await client.get({
        $includeMeta: true,
        $id: 'root',
        items: {
          name: true,
          value: true,
          $list: {
            $sort: { $field: 'startTime', $order: 'asc' },
            $find: {
              $traverse: 'children',
              $filter: [
                {
                  $field: 'startTime',
                  $operator: '>',
                  $value: 'now-6m'
                },
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
    ).items.map(i => i.name),
    ['started 5m ago', 'started 2m ago']
  )

  t.pass()

  await client.delete('root')
  await client.destroy()
})
