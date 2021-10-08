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

  await wait(100);
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

  // A small delay is needed after setting the schema
  await wait(100);

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

test.serial('slow traversal and fast index', async (t) => {
  const client = connect({ port })
  const START = 10000 // 0
  const N = 2000000 // 500000
  const STEP = 10000 // 2500

  console.log('n', 'noIndexTime', 'indexTime')
  for (let n = START; n < N; n += STEP) {
    await client.delete('root')
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

    //const expected = ((a, b) => {
    //    const arr = []
    //    for (var i = a; i <= b; i++) {
    //      if (i % 10 === 0) {
    //        arr.push(i)
    //      }
    //    }
    //    return arr
    //})(1, N + 1)

    //const filter = '#10 "value" g E L'
    const filter = `"value" g #${n / 2 - 1000} I "value" g #${n / 2 + 1000} H N`
    //const filter = `"value" g #${n / 2} F`
    const order = [] // 'order', 'value', 'asc'
    const fields = ['id'] // 'value'
    const noIndexStart = performance.now()
    for (let i = 0; i < 500; i++) {
      const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', ...order, 'fields', ...fields, 'root', filter)
      //t.deepEqual(r.map((v) => Number(v[1][1])), expected)
    }
    const noIndexEnd = performance.now()
    const noIndexTime = noIndexEnd - noIndexStart

    for (let i = 0; i < 500; i++) {
      const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', filter, ...order, 'fields', ...fields, 'root', filter)
      //t.deepEqual(r.map((v) => Number(v[1][1])), expected)
    }
    await wait(2e3)
    const indexStart = performance.now()
    for (let i = 0; i < 500; i++) {
      const r = await client.redis.selva_hierarchy_find('', '___selva_hierarchy', 'descendants', 'index', filter, ...order, 'fields', ...fields, 'root', filter)
      //t.deepEqual(r.map((v) => Number(v[1][1])), expected)
    }
    const indexEnd = performance.now()
    const indexTime = indexEnd - indexStart

    //t.deepEqual(
    //  (await client.redis.selva_index_list('___selva_hierarchy')).map((v, i) => i % 2 === 0 ? v : v[3]),
    //  [ 'root.I.IzEwICJ2YWx1ZSIgZyBFIEw=', `${expected.length}` ]
    //)

    console.log(n, noIndexTime, indexTime)
    //t.assert(noIndexTime > 2 * indexTime, 'find from index is at least twice as fast')

    // Flush indices
    const list = await client.redis.selva_index_list('___selva_hierarchy')
    for (let i = 0; i < list.length; i += 2) {
        //console.log('del', list[i])
        await client.redis.selva_index_del('___selva_hierarchy', list[i])
    }
  }
})
