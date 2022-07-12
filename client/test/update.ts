import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'
import { wait } from './assertions'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  await wait(100)
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: {},
    },
    types: {
      thing: {
        prefix: 'th',
        fields: {
          str: { type: 'string' },
          flap: { type: 'boolean' },
        },
      },
      notthing: {
        prefix: 'nh',
        fields: {
          str: { type: 'string' },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('basic batch update', async (t) => {
  const client = connect({
    port,
  })

  const id = await client.set({
    type: 'thing',
    str: 'something',
    children: [
      {
        type: 'thing',
        str: 'something',
      },
      {
        type: 'notthing',
        str: 'something',
      },
      {
        type: 'thing',
        str: 'something',
      },
    ],
  })

  await client.redis.selva_update(
    '___selva_hierarchy',
    'descendants',
    '1',
    '0',
    'str',
    'hello',
    <string>id,
    '"th" e'
  )
  t.deepEqual(
    await client.get({
      $id: id,
      rest: {
        $list: {
          $find: {
            $traverse: 'descendants',
          },
          $sort: { $field: 'type' },
        },
        type: true,
        str: true,
      },
    }),
    {
      rest: [
        { type: 'notthing', str: 'something' },
        { type: 'thing', str: 'hello' },
        { type: 'thing', str: 'hello' },
      ],
    }
  )

  await client.destroy()
})

test.serial('subscription and batch update', async (t) => {
  t.plan(3)
  const client = connect({
    port,
  })

  const id = await client.set({
    type: 'thing',
    str: 'something',
    children: [
      {
        type: 'thing',
        str: 'something',
      },
      {
        type: 'notthing',
        str: 'something',
      },
      {
        type: 'thing',
        str: 'something',
      },
    ],
  })

  const obs = client.observe({
    items: {
      type: true,
      str: true,
      $list: {
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

  let i = 0
  const sub = obs.subscribe((e) => {
    switch (i++) {
      case 0:
        t.deepEqual(e, {
          items: [
            { type: 'thing', str: 'something' },
            { type: 'thing', str: 'something' },
            { type: 'thing', str: 'something' },
          ],
        })
        break
      case 1:
        t.deepEqual(e, {
          items: [
            { type: 'thing', str: 'hello' },
            { type: 'thing', str: 'hello' },
            { type: 'thing', str: 'hello' },
          ],
        })
        break
      default:
        t.fail()
    }
  })
  await wait(100)

  await client.redis.selva_update(
    '___selva_hierarchy',
    'descendants',
    '1',
    '0',
    'str',
    'hello',
    <string>id,
    '"th" e'
  )
  t.deepEqual(
    await client.get({
      all: {
        $list: {
          $find: {
            $traverse: 'descendants',
          },
          $sort: { $field: 'type', $order: 'asc' },
        },
        type: true,
        str: true,
      },
    }),
    {
      all: [
        { type: 'notthing', str: 'something' },
        { type: 'thing', str: 'hello' },
        { type: 'thing', str: 'hello' },
        { type: 'thing', str: 'hello' },
      ],
    }
  )

  await wait(100)
  sub.unsubscribe()

  await client.destroy()
})

test.serial('update batch - api wrapper', async (t) => {
  const client = connect({
    port,
  })

  await client.set({
    type: 'thing',
    str: 'blurgh',
  })

  await client.update(
    {
      type: 'thing',
      str: 'bla',
      flap: true,
    },
    {
      $find: {
        $traverse: 'children',
        $filter: {
          $operator: '=',
          $value: 'thing',
          $field: 'type',
        },
      },
    }
  )

  const x = await client.get({
    children: {
      flap: true,
      str: true,
      $list: true,
    },
  })

  for (const thing of x.children) {
    if (!thing.flap && thing.str !== 'bla') {
      t.fail('all things need a flap and str')
    }
  }

  await client.destroy()

  t.pass('good!')
})
