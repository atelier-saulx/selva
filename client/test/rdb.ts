import { readdir, unlink } from 'node:fs/promises'
import test from 'ava'
import { join } from 'path'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait, removeDump } from './assertions'
import getPort from 'get-port'

const dir = join(process.cwd(), 'tmp', 'rdb-test')
let srv
let port: number

async function restartServer() {
  if (srv) {
    srv.destroy()
    await wait(5000)
  }

  // Delete old compressed files
  try {
    const compressedFiles = (await readdir(dir)).filter((s) => s.includes('.z'))
    await Promise.all(compressedFiles.map(async (s) => unlink(`${dir}/${s}`)))
  } catch (e) {}

  port = await getPort()
  srv = await start({
    port,
    dir,
  })
  await wait(2000)
}

test.before(removeDump(dir))
test.after(async (t) => {
    await t.connectionsAreEmpty()
    removeDump(dir)()
})

test.beforeEach(async (t) => {
  await restartServer()

  await wait(100)
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' },
          },
        },
      },
    },
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          title: { type: 'text' },
          lekkerLink: {
            type: 'reference',
            bidirectional: {
              fromField: 'lekkerLink',
            },
          },
          fren: {
            type: 'reference',
          },
          strRec: {
            type: 'record',
            values: {
              type: 'string',
            },
          },
          stringAry: {
            type: 'array',
            items: {
              type: 'string',
            },
          },
          intAry: {
            type: 'array',
            items: {
              type: 'int',
            },
          },
          doubleAry: {
            type: 'array',
            items: {
              type: 'float',
            },
          },
          objAry: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                textyText: {
                  type: 'text',
                },
                strField: {
                  type: 'string',
                },
                numField: {
                  type: 'int',
                },
              },
            },
          },
          textRec: {
            type: 'record',
            values: {
              type: 'text',
            },
          },
          objRec: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                hello: {
                  type: 'string',
                },
                value: {
                  type: 'number',
                },
                stringValue: {
                  type: 'string',
                },
              },
            },
          },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await wait(100)

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
  removeDump(dir)()
})

test.serial('can reload from RDB', async (t) => {
  let client = connect({ port })

  await client.set({
    $id: 'viTest',
    title: { en: 'hello' },
    stringAry: ['hello', 'world'],
    doubleAry: [1.0, 2.1, 3.2],
    intAry: [7, 6, 5, 4, 3, 2, 999],
    objAry: [
      {
        textyText: {
          en: 'hello 1',
          de: 'hallo 1',
        },
        strField: 'string value hello 1',
        numField: 112,
      },
      {
        textyText: {
          en: 'hello 2',
          de: 'hallo 2',
        },
        strField: 'string value hello 2',
        numField: 113,
      },
    ],
  })

  await client.set({
    $id: 'viLink1',
    title: { en: 'hi' },
    lekkerLink: {
      $id: 'viLink2',
      title: { en: 'yo' },
    },
    fren: {
      $id: 'viLink3',
      title: { en: 'sup' },
    },
  })
  await client.set({
    $id: 'viLink4',
    title: { en: 'hi' },
    parents: [],
    children: [],
    lekkerLink: {
      $id: 'viLink5',
      title: { en: 'yo' },
    },
  })

  // Compressed subtrees
  await client.set({
    $id: 'viComp1',
    title: { en: 'hello' },
    children: [
      {
        $id: 'viComp2',
        title: { en: 'hello' },
        children: [
          {
            $id: 'viComp4',
            title: { en: 'hello' },
          },
          {
            $id: 'viComp5',
            title: { en: 'hello' },
          },
        ],
      },
      {
        $id: 'viComp3',
        title: { en: 'hello' },
      },
    ],
  })
  await client.set({
    $id: 'viComp21',
    title: { en: 'hello' },
    children: [
      {
        $id: 'viComp22',
        title: { en: 'hello' },
        children: [
          {
            $id: 'viComp24',
            title: { en: 'hello' },
          },
          {
            $id: 'viComp25',
            title: { en: 'hello' },
          },
        ],
      },
      {
        $id: 'viComp23',
        title: { en: 'hello' },
      },
    ],
  })
  await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'viComp1')
  await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'viComp21')

  // Compressed subtree on disk
  await client.set({
    $id: 'viDisk1',
    title: { en: 'hello' },
    children: [
      {
        $id: 'viDisk2',
        title: { en: 'hello' },
        children: [
          {
            $id: 'viDisk4',
            title: { en: 'hello' },
          },
          {
            $id: 'viDisk5',
            title: { en: 'hello' },
          },
        ],
      },
      {
        $id: 'viDisk3',
        title: { en: 'hello' },
      },
    ],
  })
  await client.redis.selva_hierarchy_compress('___selva_hierarchy', 'viDisk1', 'disk')

  const compressedFilesBefore = (await readdir(dir)).filter((s) => s.includes('.z'))

  await client.redis.save()
  await wait(1000)

  const compressedFilesAfter = (await readdir(dir)).filter((s) => s.includes('.z'))
  t.deepEqualIgnoreOrder(compressedFilesAfter, compressedFilesBefore, 'RDB save should not remove the subtree files')

  await restartServer()
  await client.destroy()
  await wait(5000)
  client = connect({ port })

  t.deepEqual(await client.get({ $id: 'viTest', $all: true, parents: true }), {
    id: 'viTest',
    type: 'lekkerType',
    parents: ['root'],
    title: { en: 'hello' },
    stringAry: ['hello', 'world'],
    doubleAry: [1.0, 2.1, 3.2],
    intAry: [7, 6, 5, 4, 3, 2, 999],
    objAry: [
      {
        textyText: {
          en: 'hello 1',
          de: 'hallo 1',
        },
        strField: 'string value hello 1',
        numField: 112,
      },
      {
        textyText: {
          en: 'hello 2',
          de: 'hallo 2',
        },
        strField: 'string value hello 2',
        numField: 113,
      },
    ],
  })

  t.deepEqual(
    await client.get({
      $id: 'viLink1',
      $all: true,
      lekkerLink: true,
      fren: true,
    }),
    {
      id: 'viLink1',
      type: 'lekkerType',
      title: { en: 'hi' },
      lekkerLink: 'viLink2',
      fren: 'viLink3',
    }
  )
  t.deepEqual(
    await client.get({
      $id: 'viLink2',
      $all: true,
      lekkerLink: true,
      fren: true,
    }),
    {
      id: 'viLink2',
      type: 'lekkerType',
      title: { en: 'yo' },
      lekkerLink: 'viLink1',
    }
  )
  t.deepEqual(
    await client.get({
      $id: 'viLink3',
      $all: true,
      lekkerLink: true,
      fren: true,
    }),
    {
      id: 'viLink3',
      type: 'lekkerType',
      title: { en: 'sup' },
    }
  )
  t.deepEqual(
    await client.get({ $id: 'viLink4', $all: true, lekkerLink: true }),
    {
      id: 'viLink4',
      type: 'lekkerType',
      title: { en: 'hi' },
      lekkerLink: 'viLink5',
    }
  )

  // Check the compressed subtree
  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'viComp1',
      id: true,
      title: true,
      descendants: true,
    }),
    {
      id: 'viComp1',
      title: { en: 'hello' },
      descendants: ['viComp2', 'viComp3', 'viComp4', 'viComp5'],
    }
  )

  // Check the compressed subtree on disk
  // TODO Check that the compressed subtree is actually on the disk
  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'viDisk1',
      id: true,
      title: true,
      descendants: true,
    }),
    {
      id: 'viDisk1',
      title: { en: 'hello' },
      descendants: ['viDisk2', 'viDisk3', 'viDisk4', 'viDisk5'],
    }
  )
  t.deepEqual((await readdir(dir)).filter((s) => s.includes('.z')), [])

  // Do it again
  await client.redis.save()
  await wait(1000)
  await restartServer()
  await client.destroy()
  await wait(5000)
  client = connect({ port })

  t.deepEqual(await client.get({ $id: 'viTest', $all: true, parents: true }), {
    id: 'viTest',
    type: 'lekkerType',
    parents: ['root'],
    title: { en: 'hello' },
    stringAry: ['hello', 'world'],
    doubleAry: [1.0, 2.1, 3.2],
    intAry: [7, 6, 5, 4, 3, 2, 999],
    objAry: [
      {
        textyText: {
          en: 'hello 1',
          de: 'hallo 1',
        },
        strField: 'string value hello 1',
        numField: 112,
      },
      {
        textyText: {
          en: 'hello 2',
          de: 'hallo 2',
        },
        strField: 'string value hello 2',
        numField: 113,
      },
    ],
  })

  t.deepEqual(
    await client.get({
      $id: 'viLink1',
      $all: true,
      lekkerLink: true,
      fren: true,
    }),
    {
      id: 'viLink1',
      type: 'lekkerType',
      title: { en: 'hi' },
      lekkerLink: 'viLink2',
      fren: 'viLink3',
    }
  )
  t.deepEqual(
    await client.get({
      $id: 'viLink2',
      $all: true,
      lekkerLink: true,
      fren: true,
    }),
    {
      id: 'viLink2',
      type: 'lekkerType',
      title: { en: 'yo' },
      lekkerLink: 'viLink1',
    }
  )
  t.deepEqual(
    await client.get({
      $id: 'viLink3',
      $all: true,
      lekkerLink: true,
      fren: true,
    }),
    {
      id: 'viLink3',
      type: 'lekkerType',
      title: { en: 'sup' },
    }
  )

  // Check the previously compressed subtree
  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'viComp1',
      id: true,
      title: true,
      descendants: true,
    }),
    {
      id: 'viComp1',
      title: { en: 'hello' },
      descendants: ['viComp2', 'viComp3', 'viComp4', 'viComp5'],
    }
  )

  // Check the compressed subtree
  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'viComp21',
      id: true,
      title: true,
      descendants: true,
    }),
    {
      id: 'viComp21',
      title: { en: 'hello' },
      descendants: ['viComp22', 'viComp23', 'viComp24', 'viComp25'],
    }
  )

  await client.destroy()
})
