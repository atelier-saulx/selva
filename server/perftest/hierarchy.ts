import { performance } from 'perf_hooks'
import { promisify } from 'util'
import { generateTree, fieldValues } from './util/gen-tree'
import gc from './util/gc'
import newRnd, { getRandomInt } from './util/rnd'
import redis from './util/redis'

const TEST_KEY = 'test'

function calcResults(
  results: any[],
  name: string,
  nrAncestors: number[],
  tTotal: number
) {
  const n = nrAncestors.length
  const mean = nrAncestors.reduce((acc, cur) => acc + cur / n, 0)
  const stdDev = Math.sqrt(
    nrAncestors
      .map((x) => (x - mean) ** 2)
      .reduce((acc, cur) => acc + cur / (n - 1), 0)
  )

  results.push([`${name} mean(n)`, Math.round(mean)])
  results.push([`${name} σ(n)`, Math.round(stdDev)])
  results.push([`${name} t_find`, (tTotal / n).toFixed(2), 'ms/find'])
  results.push([`${name} t_total`, tTotal.toFixed(2), 'ms'])
}

export default async function hierarchy(algo: 'bfs' | 'dfs') {
  const find = promisify(redis['SELVA.HIERARCHY.find']).bind(
    redis,
    TEST_KEY,
    algo
  )
  const getFuncName = () =>
    `${new Error().stack.match(/at (\S+)/g)[1].slice(3)}_${algo}`

  // Delete an existing hierarchy and create a fresh one
  await promisify(redis.flushall).bind(redis)()
  await generateTree(redis, TEST_KEY, 3, 1, 15, 9, 0.2)
  await promisify(redis.save).bind(redis)()

  process.stderr.write('Taking a dump...')
  const fullDump = (
    await promisify(redis['SELVA.HIERARCHY.dump']).bind(redis)(TEST_KEY)
  ).map((x: string[]) => x[0])
  process.stderr.write('done\n')

  const N = 800
  const results = []
  const cases = [
    async function test_ancestors() {
      const idRnd = newRnd('totally random')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        let id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]

        const ancestors = await find('ancestors', id)
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
    async function test_descendants() {
      const idRnd = newRnd('totally random')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        let id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]

        const ancestors = await find('descendants', id)
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
    async function test_descendantsTrueFilter() {
      const idRnd = newRnd('totally random')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        const id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]
        const t = id.substring(0, 2)

        const ancestors = await find('descendants', id, `#1`)
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
    async function test_descendantsTrueFilterComplex() {
      const idRnd = newRnd('totally random')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        const id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]
        const t = id.substring(0, 2)

        const ancestors = await find('descendants', id, `#1 $1 M $1 N`, '1')
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
    async function test_descendantsWType() {
      const idRnd = newRnd('totally random')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        const id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]
        const t = id.substring(0, 2)

        const ancestors = await find('descendants', id, `"${t} e`)
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
    async function test_descendantsFieldExactStringMatch() {
      const idRnd = newRnd('totally random')
      const fieldRnd = newRnd('amazing')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        const id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]
        const v = fieldValues[getRandomInt(fieldRnd, 0, fieldValues.length)]

        const ancestors = await find('descendants', id, `"field f $1 c`, v)
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
    async function test_descendantsTypeAndBoolField() {
      const idRnd = newRnd('totally random')
      const fieldRnd = newRnd('amazing')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        const id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]
        const t = id.substring(0, 2)
        const pub = fieldRnd() < 0.5 ? 'true' : 'false'

        // Select by type and published = true/false
        const ancestors = await find(
          'descendants',
          id,
          `$1 e P "published f $2 c M`,
          t,
          pub
        )
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
    async function test_descendantsTsFieldLessThan() {
      const idRnd = newRnd('totally random')
      const fieldRnd = newRnd('amazing')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        const id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]
        const ts = getRandomInt(
          fieldRnd,
          1561634316 + (1719314360 - 1561634316) / 2,
          1719314360
        )

        // Select by type and published = true/false
        const ancestors = await find('descendants', id, `"createdAt g @1 H`, ts)
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
    async function test_descendantsBoolFieldAndTsFieldLessThan() {
      const idRnd = newRnd('totally random')
      const fieldRnd = newRnd('amazing')
      let nrAncestors = []

      const start = performance.now()
      for (let i = 0; i < N; i++) {
        const id = fullDump[getRandomInt(idRnd, 0, fullDump.length)]
        const ts = getRandomInt(fieldRnd, 1561634316, 1719314360)

        // Select by type and published = true/false
        const ancestors = await find(
          'descendants',
          id,
          `"published f "true c P "createdAt g @1 H M`,
          ts
        )
        nrAncestors.push(ancestors.length)
      }
      const end = performance.now()
      const tTotal = end - start

      calcResults(results, getFuncName(), nrAncestors, tTotal)
    },
  ]

  for (const test of cases) {
    gc()
    await test()
  }

  return results
}
