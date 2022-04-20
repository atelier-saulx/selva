import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async () => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      thing: {
        prefix: 'th',
        fields: {
          yesh: { type: 'number' },
        },
      },
    },
  })

  await wait(100)
  await client.destroy()
})

test.after(async (t) => {
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscribe and delete', async (t) => {
  const client = connect({ port })

  const q = []
  for (let i = 0; i < 10; i++) {
    q.push(
      client.set({
        type: 'thing',
        yesh: i,
      })
    )
  }

  const ids = await Promise.all(q)

  let cnt = 0
  const observable = client.observe({
    $id: 'root',
    things: {
      id: true,
      yesh: true,
      $list: {
        $find: {
          $traverse: 'children', // also desc
          $filter: {
            $operator: '=',
            $value: 'thing',
            $field: 'type',
          },
        },
      },
    },
  })

  const s = observable.subscribe((d) => {
    cnt++
    console.info(d)
  })

  await wait(1000)

  await client.set({ type: 'thing', yesh: 2 })

  await wait(1000)

  await client.delete({ $id: ids[0] })

  await wait(1000)

  t.is(cnt, 3)

  s.unsubscribe()

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})

test.serial.only('subscribe and delete descendant', async (t) => {
  const client = connect({ port })

  console.log('HELLo')
  const id = await client.set({
    type: 'thing',
    yesh: 1,
    children: [
      {
        type: 'thing',
        $id: 'th2',
        yesh: 2,
      }
    ],
  })

  console.log('sub to id:', id)
  const observable = client.observe({
    $id: id,
      $language: "en",
      items: {
        //parents: true,
          id: true,
          $list: {
            $limit: 1000,
              $offset: 0,
              $sort: {
                $field: "createdAt",
                  $order: "desc",
              },
              $find: {
                $traverse: "descendants",
                  $filter: [
                    {
                      $field: "type",
                      $operator: "=",
                      $value: 'thing',
                    },
                  ],
              },
          },
      },
  })

  observable.subscribe((v) => {
    console.log('sub got:', v)
  })

  await wait(100)
  await client.delete('th2')
  await wait(100)
  console.log('Deleted')

  console.log(await client.get({ $id: 'root', descendants: true }))

  await wait(2e3)
  await client.destroy()
})
