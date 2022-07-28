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

test.serial('find - traverse expression - low level', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

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

  await client.destroy()
})

test.serial.only('find - traverse expression with records', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      book: {
        prefix: 'bk',
        fields: {
          name: { type: 'string' },
          revisions: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                version: { type: 'string' },
                publishedAt: { type: 'timestamp' },
                contents: { type: 'reference' },
              },
            },
          },
        },
      },
      section: {
        prefix: 'sc',
        fields: {
          name: { type: 'text' },
          text: { type: 'text' },
          revisionedChildren: {
            type: 'record',
            values: {
              type: 'references'
            },
          },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  const book = await client.set({
    $language: 'en',
    type: 'book',
    name: 'Liber Optimus',
    revisions: [
      {
        version: 'v1',
        publishedAt: (new Date('2000')).getTime(),
        contents: {
          type: 'section',
          $id: 'sc1',
          name: 'Preface',
          text: 'Neque porro quisquam est qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit...',
          revisionedChildren: {
            v1: [
              {
                $id: 'sc2',
                type: 'section',
                name: '1. Prologue',
                text: 'Lorem ipsum dolor sit amet, consectetur adipiscing elit.',
              },
              {
                $id: 'sc3',
                type: 'section',
                name: '5. Epilogue',
                text: 'Quis autem vel eum iure reprehenderit qui in ea voluptate velit esse quam nihil molestiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla pariatur?',
              },
            ],
          },
        },
      },
    ],
  })

  // TODO reference field in an object in an array connot be visited with SELVA_HIERARCHY_TRAVERSAL_ARRAY
  //const preface = await client.get({
  //  $id: book,
  //  id: true,
  //  revisions: {
  //    version: true,
  //    contents: true,
  //    $list: true
  //  },
  //})

  const sections = await client.get({
    $id: 'sc1',
    name: true,
    revisionedChildren: {
      '*': {
        $all: true,
        $list: true,
      },
    },
  })
  console.log(sections)


  await client.destroy()
})
