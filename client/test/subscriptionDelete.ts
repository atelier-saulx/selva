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
          next: { type: 'reference' },
          things: { type: 'references' },
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

test.serial('subscribe and delete a descendant', async (t) => {
  const client = connect({ port })

  const id = await client.set({
    type: 'thing',
    yesh: 1,
    children: [
      {
        type: 'thing',
        $id: 'th2',
        yesh: 2,
      },
    ],
  })

  const observable = client.observe({
    $id: id,
    $language: 'en',
    items: {
      id: true,
      $list: {
        $limit: 1000,
        $offset: 0,
        $sort: {
          $field: 'createdAt',
          $order: 'desc',
        },
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'thing',
            },
          ],
        },
      },
    },
  })

  t.plan(2)
  let i = 0
  observable.subscribe((v) => {
    switch (i++) {
      case 0:
        t.deepEqual(v, { items: [{ id: 'th2' }] })
        break
      case 1:
        t.deepEqual(v, { items: [] })
        break
    }
  })

  await wait(100)
  await client.delete('th2')
  await wait(100)

  await client.destroy()
})

test.serial('subscribe and delete over a reference field', async (t) => {
  const client = connect({ port })

  const id = await client.set({
    type: 'thing',
    yesh: 1,
    next: {
      type: 'thing',
      $id: 'th2',
      yesh: 2,
    },
  })

  const observable = client.observe({
    $id: id,
    $language: 'en',
    items: {
      id: true,
      $list: {
        $limit: 1000,
        $offset: 0,
        $sort: {
          $field: 'createdAt',
          $order: 'desc',
        },
        $find: {
          $traverse: 'next',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'thing',
            },
          ],
        },
      },
    },
  })

  t.plan(2)
  let i = 0
  observable.subscribe((v) => {
    switch (i++) {
      case 0:
        t.deepEqual(v, { items: [{ id: 'th2' }] })
        break
      case 1:
        t.deepEqual(v, { items: [] })
        break
    }
  })

  await wait(100)
  await client.delete('th2')
  await wait(100)

  await client.destroy()
})

test.serial('subscribe and delete over references field', async (t) => {
  const client = connect({ port })

  const id = await client.set({
    type: 'thing',
    yesh: 1,
    things: [
      {
        type: 'thing',
        $id: 'th2',
        yesh: 2,
      },
      {
        type: 'thing',
        $id: 'th3',
        yesh: 3,
      },
    ],
  })

  const observable = client.observe({
    $id: id,
    $language: 'en',
    items: {
      id: true,
      $list: {
        $limit: 1000,
        $offset: 0,
        $sort: {
          $field: 'createdAt',
          $order: 'desc',
        },
        $find: {
          $traverse: 'things',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'thing',
            },
          ],
        },
      },
    },
  })

  t.plan(2)
  let i = 0
  observable.subscribe((v) => {
    switch (i++) {
      case 0:
        t.deepEqualIgnoreOrder(v, { items: [{ id: 'th2' }, { id: 'th3' }] })
        break
      case 1:
        t.deepEqual(v, { items: [{ id: 'th3' }] })
        break
    }
  })

  await wait(100)
  await client.delete('th2')
  await wait(100)

  await client.destroy()
})

test.serial('subscribe and delete one item', async (t) => {
  const client = connect({ port })
  let cnt = 0
  const observable = client.observe({
    $id: 'thing1',
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
    cnt++ // 1
  })

  await wait(1000)

  const id = (await client.set({
    type: 'thing',
    yesh: 12,
    parents: ['thing1']
  })) as string // 2
  await wait(1000)
  await client.delete({ $id: id }) // 3
  await wait(1000)

  t.is(cnt, 3)

  s.unsubscribe()
  await client.delete('root')
  await wait(1000)
  await client.destroy()
})

test.serial('subscribe and delete one item: root', async (t) => {
  const client = connect({ port })
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
    cnt++ // 1
  })

  await wait(1000)

  const id = (await client.set({
    type: 'thing',
    yesh: 12,
  })) as string // 2
  await wait(1000)
  await client.delete({ $id: id }) // 3
  await wait(1000)

  t.is(cnt, 3)

  s.unsubscribe()
  await client.delete('root')
  await wait(1000)
  await client.destroy()
})
