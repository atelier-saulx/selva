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
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.redis.flushall()
})

test.after(async (t) => {
  const client = connect({ port: port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - inherit - low level', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      book: {
        prefix: 'bk',
        fields: {
          name: { type: 'string' },
        },
      },
      author: {
        prefix: 'au',
        fields: {
          fullname: { type: 'string' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.set({
    type: 'author',
    $id: 'au1',
    fullname: 'John Doe',
    children: [
      {
        type: 'book',
        $id: 'bk1',
        name: 'The Best Book',
      },
      {
        type: 'book',
        $id: 'bk2',
        name: 'The Worst Book',
      },
    ],
  })
  await client.set({
    type: 'author',
    $id: 'au2',
    fullname: 'Jane Doe',
    children: [
      {
        type: 'book',
        $id: 'bk3',
        name: 'Unfunny Book',
      },
    ],
  })

  const filter = '"bk" e'
  t.deepEqual(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'descendants',
      'inherit_rpn',
      '{"name","^:fullname"}',
      'root',
      filter
    ),
    [
      ['bk1', ['name', 'The Best Book', 'fullname', ['au1', 'John Doe']]],
      ['bk2', ['name', 'The Worst Book', 'fullname', ['au1', 'John Doe']]],
      ['bk3', ['name', 'Unfunny Book', 'fullname', ['au2', 'Jane Doe']]],
    ]
  )
  t.deepEqual(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'descendants',
      'order',
      'fullname',
      'asc',
      'inherit_rpn',
      '{"name","^:fullname"}',
      'root',
      filter
    ),
    [
      ['bk3', ['name', 'Unfunny Book', 'fullname', ['au2', 'Jane Doe']]],
      ['bk1', ['name', 'The Best Book', 'fullname', ['au1', 'John Doe']]],
      ['bk2', ['name', 'The Worst Book', 'fullname', ['au1', 'John Doe']]],
    ]
  )

  await client.destroy()
})

test.serial('find - inherit', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      book: {
        prefix: 'bk',
        fields: {
          name: { type: 'string' },
        },
      },
      author: {
        prefix: 'au',
        fields: {
          fullname: { type: 'string' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.set({
    type: 'author',
    $id: 'au1',
    fullname: 'John Doe',
    children: [
      {
        type: 'book',
        $id: 'bk1',
        name: 'The Best Book',
      },
      {
        type: 'book',
        $id: 'bk2',
        name: 'The Worst Book',
      },
    ],
  })
  await client.set({
    type: 'author',
    $id: 'au2',
    fullname: 'Jane Doe',
    children: [
      {
        type: 'book',
        $id: 'bk3',
        name: 'Unfunny Book',
      },
    ],
  })

  t.deepEqual(
    await client.get({
      books: {
        name: true,
        fullname: { $inherit: true },
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: {
              $field: 'type',
              $operator: '=',
              $value: 'book',
            },
          },
        },
      },
    }),
    {
      books: [
        {
          name: 'The Best Book',
          fullname: 'John Doe',
        },
        {
          name: 'The Worst Book',
          fullname: 'John Doe',
        },
        {
          name: 'Unfunny Book',
          fullname: 'Jane Doe',
        },
      ],
    }
  )

  await client.destroy()
})

test.serial('find - inherit by type', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      book: {
        prefix: 'bk',
        fields: {
          name: { type: 'string' },
        },
      },
      author: {
        prefix: 'au',
        fields: {
          fullname: { type: 'string' },
        },
      },
      fakeAuthor: {
        prefix: 'fa',
        fields: {
          fullname: { type: 'string' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.set({
    type: 'author',
    $id: 'au1',
    fullname: 'John Doe',
    children: [
      {
        $id: 'fa1',
        fullname: 'Fake Author 1',
        children: [
          {
            type: 'book',
            $id: 'bk1',
            name: 'The Best Book',
          },
          {
            type: 'book',
            $id: 'bk2',
            name: 'The Worst Book',
          },
        ],
      },
    ],
  })
  await client.set({
    type: 'author',
    $id: 'au2',
    fullname: 'Jane Doe',
    children: [
      {
        $id: 'fa2',
        fullname: 'Fake Author 2',
        children: [
          {
            type: 'book',
            $id: 'bk3',
            name: 'Unfunny Book',
          },
        ],
      },
    ],
  })

  t.deepEqual(
    await client.get({
      books: {
        name: true,
        fullname: { $inherit: { $type: ['author', 'root'] } },
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: {
              $field: 'type',
              $operator: '=',
              $value: 'book',
            },
          },
        },
      },
    }),
    {
      books: [
        {
          name: 'The Best Book',
          fullname: 'John Doe',
        },
        {
          name: 'The Worst Book',
          fullname: 'John Doe',
        },
        {
          name: 'Unfunny Book',
          fullname: 'Jane Doe',
        },
      ],
    }
  )

  await client.destroy()
})
