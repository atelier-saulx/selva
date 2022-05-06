import { join as pathJoin } from 'path'
import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait, removeDump } from './assertions'
import getPort from 'get-port'

let srv
let port: number
const dir = pathJoin(process.cwd(), 'tmp', 'compression-test')

async function restartServer() {
  if (srv) {
    srv.destroy()
    await wait(5000)
  }

  port = await getPort()
  srv = await start({
    port,
    selvaOptions: ['HIERARCHY_AUTO_COMPRESS_PERIOD_MS', '100', 'HIERARCHY_AUTO_COMPRESS_OLD_AGE_LIM', '100'],
    dir,
  })
}

test.before(async (t) => {
  removeDump(dir)()
  port = await getPort()
  srv = await start({
    port,
    dir,
  })
  await wait(100)
})

test.beforeEach(async (t) => {
  await restartServer()

  await wait(100)
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
      },
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' },
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
  removeDump(dir)()
})

test.serial('simple auto compress', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    children: [
      {
        type: 'match',
        title: { en: 'hello' },
        value: 11,
        children: [
          {
            type: 'match',
            title: { en: 'last' },
          }
        ]
      },
    ],
  })
  await client.set({
    $id: 'ma2',
    title: { de: 'hallo' },
    value: 10,
    children: [
      {
        type: 'match',
        title: { en: 'hello' },
        value: 11,
        children: [
          {
            type: 'match',
            title: { en: 'last' },
          }
        ]
      },
    ],
  })

  for (let i = 0; i < 200; i++) {
    await client.get({
      $id: 'ma2',
      descendants: true,
    })
  }
  await client.redis.save()
  await wait(1000)
  const compressedList = await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy')
  t.truthy(compressedList.includes('ma1'))

  client.destroy()
})

test.serial.only('a lot of nodes to be compressed', async (t) => {
  const NR_HEADS = 410
  const client = connect({ port })

  await client.set({
    $id: 'ma1',
    title: { de: 'hallo' },
    value: 10,
    children: [
      {
        type: 'match',
        title: { en: 'hello' },
        value: 11,
        children: [
          {
            type: 'match',
            title: { en: 'last' },
          }
        ]
      },
    ],
  })

  let expected = []
  for (let i = 0; i < NR_HEADS; i++) {
    await client.set({
      type: 'match',
      $id: `maA${i}`,
      children: [
        {
          type: 'match',
          $id: `maB${i}`,
          children: [
            {
              type: 'match',
              $id: `maC${i}`,
              children: [
                {
                  type: 'match',
                  $id: `maD${i}`,
                }
              ]
            }
          ]
        }
      ]
    })
    expected.push(`maA${i}`, `maB${i}`, `maC${i}`, `maD${i}`)
  }
  expected.sort()

  for (let i = 0; i < (NR_HEADS * 4) / (4096 / 10 | 0); i++) {
    for (let i = 0; i < 200; i++) {
      await client.get({
        $id: 'ma1',
        descendants: true,
      })
    }

    await client.redis.save()
    await wait(500)
  }

  const compressedList = await client.redis.selva_hierarchy_listcompressed('___selva_hierarchy')
  compressedList.sort()
  t.falsy(compressedList.includes('ma1'))
  t.deepEqual(compressedList, expected)
  //console.log('exp', expected)
  //console.log('act', compressedList)
  //console.log(compressedList.length)

  client.destroy()
})
