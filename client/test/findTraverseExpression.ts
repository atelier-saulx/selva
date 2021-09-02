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
  await client.updateSchema({
    languages: ['en'],
    types: {
      library: {
        prefix: 'li',
        fields: {
          name: { type: 'string' },
          books: {
            type: 'references',
            bidirectional: {
              fromField: 'library',
            },
          },
        },
      },
      book: {
        prefix: 'bk',
        fields: {
          name: { type: 'string' },
          library: {
            type: 'reference',
            bidirectional: {
              fromField: 'books',
            },
          },
          author: { type: 'reference' },
          publisher: {
            type: 'reference',
            bidirectional: {
              fromField: 'books',
            },
          },
          publishedAt: { type: 'timestamp' },
        },
      },
      publisher: {
        prefix: 'pb',
        fields: {
          name: { type: 'string' },
          books: {
            type: 'references',
            bidirectional: {
              fromField: 'publisher',
            },
          },
        },
      },
      author: {
        prefix: 'au',
        fields: {
          books: {
            type: 'references',
            bidirectional: {
              fromField: 'author',
            },
          },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  const alexandria = await client.set({
    type: 'publisher',
    $id: 'pb08523c44',
    name: 'The Great Library of Alexandria in Alexandria',
  })
  const agrippina = await client.set({
    type: 'author',
    $id: 'aud11d986e',
    name: 'Agrippina the Younger',
  })
  const democritus = await client.set({
    type: 'author',
    $id: 'au3c163ed6',
    name: 'Democritus',
  })

  const date = new Date()
  date.setMonth(0)
  date.setDate(1)

  await client.set({
    type: 'library',
    name: 'The Great Library of Alexandria in Alexandria',
    books: [
      {
        type: 'book',
        $id: 'bk5b0985b0',
        name: 'Septuagint',
        publisher: alexandria,
        publishedAt: date.setFullYear(-305),
      },
      {
        type: 'book',
        $id: 'bkcbbde08f',
        name: 'Geometrical Reality',
        author: democritus,
        publishedAt: date.setFullYear(-460),
      },
      {
        type: 'book',
        $id: 'bkcfcc6a0d',
        name: 'Geometrical Reality',
        author: democritus,
        publishedAt: date.setFullYear(-460),
      },
      {
        type: 'book',
        $id: 'bkf38955bb',
        name: 'Casus Suorum',
        author: agrippina,
        publisher: alexandria,
        publishedAt: date.setFullYear(50),
      },
    ],
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - traverse expression', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  const traversal =
    '{"children"} {"author","publisher"} j "bk" e T {"books"} "li" e T'
  const filter = '$0 b {"bk","au","pb"} a'
  t.deepEqual(
    await client.redis.selva_hierarchy_find(
      '',
      '___selva_hierarchy',
      'bfs_expression',
      traversal,
      'order',
      'type',
      'asc',
      'fields',
      'type\nname\nauthor|publisher',
      'root',
      filter
    ),
    [
      ['au3c163ed6', ['type', 'author', 'name', 'Democritus']],
      ['aud11d986e', ['type', 'author', 'name', 'Agrippina the Younger']],
      [
        'bk5b0985b0',
        ['type', 'book', 'name', 'Septuagint', 'publisher', ['pb08523c44']],
      ],
      [
        'bkcbbde08f',
        [
          'type',
          'book',
          'name',
          'Geometrical Reality',
          'author',
          ['au3c163ed6'],
        ],
      ],
      [
        'bkcfcc6a0d',
        [
          'type',
          'book',
          'name',
          'Geometrical Reality',
          'author',
          ['au3c163ed6'],
        ],
      ],
      [
        'bkf38955bb',
        ['type', 'book', 'name', 'Casus Suorum', 'author', ['aud11d986e']],
      ],
      [
        'pb08523c44',
        [
          'type',
          'publisher',
          'name',
          'The Great Library of Alexandria in Alexandria',
        ],
      ],
    ]
  )

  await client.delete('root')
  await client.destroy()
})
