import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {},
      },
      team: {
        prefix: 'te',
        fields: {
          value: { type: 'number' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          value: { type: 'number' },
          status: { type: 'number' },
        },
      },
      thing: {
        prefix: 'th',
        fields: {
          docs: { type: 'references' },
        },
      },
      file: {
        prefix: 'tx',
        fields: {
          name: { type: 'string' },
          mirrors: { type: 'references' },
        },
      },
      mirror: {
        prefix: 'sp',
        fields: {
          url: { type: 'string' },
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

test.serial.only('retrieving nested refs with fields arg', async (t) => {
  const client = connect({ port })

  for (let i = 0; i < 2; i++) {
    await client.set({
      type: 'thing',
      docs: [...Array(2)].map((_, i) => ({
        type: 'file',
        $id: `tx${i}`,
        name: `file${i}.txt`,
        mirrors: [
          {
            type: 'mirror',
            url: `http://localhost:3000/file${i}.txt`,
          },
          {
            type: 'mirror',
            url: `http://localhost:3001/file${i}.txt`,
          },
        ]
      }))
    })
  }

  const res1 = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'limit', '1', 'fields', 'docs.*', 'root', '"th" e')
  t.deepEqualIgnoreOrder(res1[0][1], [
    "docs",
    [
      [
        "id",
        "tx0",
        "name",
        "file0.txt",
        "type",
        "file"
      ],
      [
        "id",
        "tx1",
        "name",
        "file1.txt",
        "type",
        "file"
      ]
    ]
  ])

  const res2 = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'limit', '1', 'fields', 'docs.name\ndocs.mirrors.url', 'root', '"th" e')
  t.deepEqual(res2[0][1][0], 'docs')
  t.deepEqual(res2[0][1][1], [
   [
     "name",
     "file0.txt"
   ],
   [
     "name",
     "file1.txt"
   ]
 ])
 t.deepEqual(res2[0][1][2], 'docs')
 t.truthy(res2[0][1][3][0].length === 2)
 t.deepEqual(res2[0][1][3][0][0], 'mirrors')
 t.deepEqual(res2[0][1][3][0][1][0][0], 'url')
 t.deepEqual(res2[0][1][3][1][0], 'mirrors')

  const res3 = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'limit', '1', 'fields', 'docs.name\ndocs.mirrors.url\n!docs.mirrors.url', 'root', '"th" e')
  t.deepEqualIgnoreOrder(res3[0][1], [
    "docs",
    [
      [
        "name",
        "file0.txt"
      ],
      [
        "name",
        "file1.txt"
      ]
    ]
  ])

  const res4 = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'limit', '1', 'fields', 'id\ndocs.id\ndocs.name\ndocs.mirrors.*\n!id', 'root', '"th" e')
  t.deepEqual(res4[0][1][0], 'id', 'id not excluded')

  // Excluding fields over a multi-edge field doesn't currently work
  const res5= await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'fields', 'docs.id\ndocs.name\ndocs.mirrors.*\n!docs.mirrors.id', 'root', '"th" e')
  t.deepEqual(res5[0][1][0], 'docs')
  t.deepEqual(res5[0][1][1][0][0], 'id') // hence we have id here anyway
  t.deepEqual(res5[0][1][1][1][0], 'id')
  t.deepEqual(res5[0][1][2], 'docs')
  t.deepEqual(res5[0][1][3][0][0], 'name')
  t.deepEqual(res5[0][1][3][0][1], 'file0.txt')
  t.deepEqual(res5[0][1][3][1][0], 'name')
  t.deepEqual(res5[0][1][3][1][1], 'file1.txt')
  t.deepEqual(res5[0][1][4], 'docs')
  t.deepEqual(res5[0][1][5][0][0], 'mirrors')
  t.truthy(res5[0][1][5][0][1].length === 2)
  t.truthy(res5[0][1][5][0][1][0].length === 6)
  t.truthy(res5[0][1][5][0][1][1].length === 6)
  t.deepEqual(res5[1][1][0], 'docs')
  t.deepEqual(res5[1][1][1][0][0], 'id')
  t.deepEqual(res5[1][1][1][1][0], 'id')
  t.deepEqual(res5[1][1][2], 'docs')
  t.deepEqual(res5[1][1][3][0][0], 'name')
  t.deepEqual(res5[1][1][3][0][1], 'file0.txt')
  t.deepEqual(res5[1][1][3][1][0], 'name')
  t.deepEqual(res5[1][1][3][1][1], 'file1.txt')
  t.deepEqual(res5[1][1][4], 'docs')
  t.deepEqual(res5[1][1][5][0][0], 'mirrors')
  t.truthy(res5[1][1][5][0][1].length === 2)
  t.truthy(res5[1][1][5][0][1][0].length === 6)
  t.truthy(res5[1][1][5][0][1][1].length === 6)

  await client.delete('root')
  await client.destroy()
})
