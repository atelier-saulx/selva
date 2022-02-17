import { open } from 'fs/promises'
import test from 'ava'
import { performance } from 'perf_hooks'
import { connect } from '../../src/index'
import { start } from '@saulx/selva-server'
import '../assertions'
import { wait } from '../assertions'
import getPort from 'get-port'

let srv
let port: number

const toCArr = (arr) => arr.map(s => s.padEnd(10, '\0')).join('')

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
    selvaOptions: ['FIND_INDICES_MAX', '100', 'FIND_INDEXING_INTERVAL', '1000', 'FIND_INDEXING_ICB_UPDATE_INTERVAL', '500', 'FIND_INDEXING_POPULARITY_AVE_PERIOD', '3', 'FIND_INDEXING_THRESHOLD', '0'],
  })

  await wait(100)
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
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

test.afterEach(async (t) => {
  const client = connect({ port: port })
  await new Promise((r) => setTimeout(r, 100))
  await client.delete('root')
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial.failing('perf: indexing', async (t) => {
  const START = 0 // Graph size factor in the first step
  const STOP = 800000 // Graph size factor in the last step
  const NR_STEPS = 8 // Number of steps between START and STOP
  const STEP = Math.round((STOP - START) / NR_STEPS) // Step size
  const ITER = 500 // Number of iterations

  if (STEP <= 0) {
      throw new Error('STEP must be greater than zero')
  }
  console.error('STEP:', STEP)

  const client = connect({ port })
  const file = await open(`perf-indexing_${new Date().toISOString()}.csv`, 'w')
  const writeLine = async (...l) => {
    console.log(l)
    await file.write(Buffer.from(l.join(',') + '\n'))
  }

  await writeLine('n', 'nrNodes', 'resultLen', 'noIndexTime', 'indexTime')

  for (let n = START; n <= STOP; n += STEP) {
    // Format the query
    const filter = `"ma" e P #${n / 2 + 999} "value" g #${n / 2 - 1000} i #2 "value" g E L M M`
    const indexFilter = ['"ma" e P', `#${n / 2 + 999} "value" g #${n / 2 - 1000} i`, '#2 "value" g E L'].map((s) => ['index', s]).flat()
    const order = [] // 'value', 'asc'
    const fields = [] // 'value'
    const baseQuery = [
        ...(order.length > 0 ? ['order', ...order] : []),
        ...(fields.length > 0 ? ['fields', fields.join('\n')] : []),
        'root',
        filter,
    ]

    // Delete all nodes.
    // This works if all nodes were created as descendants of the root nodes.
    await client.delete('root')

    // Create nodes
    console.error('n:', n)
    for (let i = 1; i < n + 1; i++) {
      await client.set({
        type: 'match',
        id: `ma${i}`,
        value: i,
        parents: [ `ma${i - 1}` ],
        children: [
          {
            type: 'match',
            value: i | 1,
          }
        ]
      })
    }
    console.error('nodes created')

    // nr of match nodes
    const nrNodes = (await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'root', '"ma" e')).length

    // Test without indexing
    console.error('test without indexing')
    let resultLen
    const noIndexStart = performance.now()
    for (let i = 0; i < ITER; i++) {
      const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', ...baseQuery)
      resultLen = r.length
    }
    const noIndexEnd = performance.now()
    const noIndexTime = noIndexEnd - noIndexStart

    // Ensure that the index is created before the test run
    console.error('create index')
    for (let i = 0; i < ITER; i++) {
      const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', ...indexFilter, ...baseQuery)
    }
    await wait(2e3)

    // Test with indexing
    console.error('test with indexing')
    const indexStart = performance.now()
    for (let i = 0; i < ITER; i++) {
      const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', ...indexFilter, ...baseQuery)
    }
    const indexEnd = performance.now()
    const indexTime = indexEnd - indexStart

    await writeLine(n, nrNodes, resultLen, noIndexTime, indexTime)

    // Flush indices
    const list = await client.redis.selva_index_list('___selva_hierarchy')
    for (let i = 0; i < list.length; i += 2) {
        //console.log('del', list[i])
        await client.redis.selva_index_del('___selva_hierarchy', list[i])
    }
  }

  await file.close()
  t.pass()
})
