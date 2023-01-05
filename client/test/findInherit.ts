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
      }
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
      [
        'bk1',
        [
          'name',
          'The Best Book',
          'fullname',
          [
            'au1',
            'John Doe',
          ],
        ],
      ],
      [
        'bk2',
        [
          'name',
          'The Worst Book',
          'fullname',
          [
            'au1',
            'John Doe',
          ],
        ],
      ],
      [
        'bk3',
        [
          'name',
          'Unfunny Book',
          'fullname',
          [
            'au2',
            'Jane Doe',
          ],
        ],
      ],
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
      [
        'bk3',
        [
          'name',
          'Unfunny Book',
          'fullname',
          [
            'au2',
            'Jane Doe',
          ],
        ],
      ],
      [
        'bk1',
        [
          'name',
          'The Best Book',
          'fullname',
          [
            'au1',
            'John Doe',
          ],
        ],
      ],
      [
        'bk2',
        [
          'name',
          'The Worst Book',
          'fullname',
          [
            'au1',
            'John Doe',
          ],
        ],
      ],
    ]
  )

  await client.destroy()
})
