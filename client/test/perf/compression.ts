import { join } from 'path'
import { performance } from 'perf_hooks'
import test from 'ava'
import { connect } from '../../src/index'
import { start } from '@saulx/selva-server'
import { wait, removeDump } from '../assertions'
import getPort from 'get-port'

const N = 100;
let srv
let port: number
const dir = join(process.cwd(), 'tmp', 'compression-perf-test')

test.before(async (t) => {
  removeDump(dir)()
  port = await getPort()
  srv = await start({
    port,
    dir,
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string' },
        },
      },
      club: {
        prefix: 'cl',
        fields: {
          name: { type: 'string' },
        },
      },
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          flupriflu: { type: 'string' },
          date: { type: 'number' },
          value: { type: 'number' },
          status: { type: 'number' },
        },
      },
      video: {
        prefix: 'vi',
        fields: {
          title: { type: 'text' },
          date: { type: 'number' },
          value: { type: 'number' },
        },
      },
    },
  })

  const amount = 100
  const genVideos = (n) => {
    const ch = []
    for (let i = 0; i < n; i++) {
      ch.push({
        type: 'video',
        name: 'video',
        title: { en: `Game video ${i}` },
        date: Date.now() + i + (i > 5 ? 1000000 : -100000),
        value: i,
      })
    }
    return ch
  }
  const genMatches = () => {
    const ch = []
    for (let i = 0; i < amount; i++) {
      for (let j = 0; j < amount; j++) {
        if (i == j) continue
        ch.push({
          type: 'match',
          flupriflu: 'true',
          name: `match team${i} vs team${j}`,
          status: i === 0 ? 2 : i > 1000 ? 100 : 300,
          parents: [ `team${i}`, `team${j}` ],
          children: genVideos(4),
        })
      }
    }

    return ch
  }

  const d = performance.now()
  await Promise.all([
    client.set({
      type: 'league',
      name: 'league 1',
      // @ts-ignore
      children: genMatches(),
    }),
  ])
  const tEnd = performance.now() - d
  const nrIds = (await client.get({ descendants: true })).descendants.length
  console.info(`Created a hierarchy with ${nrIds} nodes in ${tEnd} ms`)

  await wait(600)
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

test.serial.failing('perf: find compression perf - descendants', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await wait(2e3)

  const ids = (
    await client.get({
      n: {
        id: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: {
              $field: 'type',
              $operator: '=',
              $value: 'match',
            },
          },
        },
      },
    })
  ).n.map(({ id }) => id)
  const q = {
    items: {
      name: true,
      value: true,
      status: true,
      date: true,
      children: true,
      $list: {
        $sort: { $field: 'status', $order: 'desc' },
        $limit: 1000,
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $operator: '=',
              $field: 'type',
              $value: 'match',
              $and: {
                $operator: '=',
                $field: 'status',
                $value: [300, 2],
              },
            },
          ],
        },
      },
    },
  }

  const compDecimator = 2
  const compressNodes = async (ids: string[], type: string) => {
    const startCompress = performance.now()

    await Promise.all(
      ids.filter((_, i: number) => i % compDecimator == 0).map(async (id: string) => {
        try {
          await client.redis.selva_hierarchy_compress('___selva_hierarchy', id)
        } catch (e) { console.error(e) }
      })
    )

    return performance.now() - startCompress
  }

  // Q1
  const q1Start = performance.now()
  for (let i = 0; i < N; i++) {
    await client.get(q)
  }
  const q1TimeTotal = performance.now() - q1Start

  // Q2
  const q2Start = performance.now()
  let q2CompTimeTotal = 0;
  for (let i = 0; i < N; i++) {
    q2CompTimeTotal += await compressNodes(ids, 'mem')
    await client.get(q)
  }
  const q2TimeTotal = performance.now() - q2Start - q2CompTimeTotal

  // Q3
  const q3Start = performance.now()
  let q3CompTimeTotal = 0;
  for (let i = 0; i < N; i++) {
    q3CompTimeTotal += await compressNodes(ids, 'disk')
    await client.get(q)
  }
  const q3TimeTotal = performance.now() - q3Start - q3CompTimeTotal

  // Results
  console.info('N', N)
  console.info('normal query', q1TimeTotal, 'ms')
  console.info('compression total (mem)', q2CompTimeTotal, 'ms')
  console.info('uncompressing query (mem)', q2TimeTotal, 'ms')
  console.info('compression total (disk)', q3CompTimeTotal, 'ms')
  console.info('uncompressing query (disk)', q3TimeTotal, 'ms')
})
