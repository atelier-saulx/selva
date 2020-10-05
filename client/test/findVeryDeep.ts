import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait, worker } from './assertions'
import getPort from 'get-port'
import chalk from 'chalk'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en'],
    types: {
      glurp: {
        prefix: 'gl',
        fields: {
          levelCnt: { type: 'number' },
          title: { type: 'string' }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('get very deep results', async t => {
  const client = connect({ port })

  const q: any = {}
  let s: any = q

  const setObj: any = {}
  const levels = 14
  const amount = 2

  for (let i = 0; i < levels; i++) {
    s.$find = {
      $traverse: 'children',
      $filter: {
        $field: 'type',
        $operator: '=',
        $value: 'glurp'
      }
    }
    s = s.$find
  }

  const levelMap = {}

  const recurse = (x: any, i = 0) => {
    let myLevel = i
    if (!levelMap[myLevel]) {
      levelMap[myLevel] = 0
    }
    if (i < levels) {
      x.children = []
      const nextI = i + 1
      for (let j = 0; j < amount; j++) {
        ++levelMap[myLevel]
        let n: any = {
          type: 'glurp',
          levelCnt: levelMap[myLevel],
          title: `Level ${myLevel} child ${j} level count -> ${levelMap[myLevel]}`
        }
        x.children.push(n)
        recurse(n, nextI)
      }
    }
  }
  recurse(setObj)

  setObj.$id = 'root'

  var d = Date.now()
  await client.set(setObj)
  console.log(
    chalk.gray(`    Set ${amount}^${levels} things ${Date.now() - d} ms`)
  )

  const myQuery = {
    x: {
      //   title: true,
      //   id: true,
      levelCnt: true,
      $list: {
        $find: q.$find
      }
    }
  }

  //   console.dir(myQuery, { depth: 100 })

  d = Date.now()
  const ultraResults = await client.get(myQuery)
  console.log(
    chalk.gray(
      `    Get ${amount}^${levels +
        1} things using nested queries in ${Date.now() - d} ms`
    )
  )

  //   console.dir(ultraResults, { depth: 10 })

  const r = []

  for (let i = 0; i < levelMap[levels - 1]; i++) {
    r.push({
      levelCnt: i + 1
    })
  }

  t.deepEqualIgnoreOrder(
    ultraResults.x,
    r,
    `has correct amount of result (${levelMap[levels - 1]}) for ${levels +
      1} deep`
  )

  d = Date.now()
  const x2 = await client.get({
    x: {
      levelCnt: true,
      title: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $operator: '=',
            $field: 'type',
            $value: 'glurp'
          }
        }
      }
    }
  })

  console.log(
    chalk.gray(`    Get all desc using descendants in ${Date.now() - d} ms`)
  )

  t.is(x2.x.length, 32766)

  const workers = []

  const workerAmount = 10

  for (let i = 0; i < workerAmount; i++) {
    workers.push(
      worker(
        async ({ connect, wait }, { port }) => {
          const client = connect({ port })
          var d = Date.now()
          const x = await client.get({
            x: {
              levelCnt: true,
              $list: {
                $find: {
                  $traverse: 'descendants',
                  $filter: [
                    {
                      $operator: '=',
                      $field: 'type',
                      $value: 'glurp'
                    },
                    {
                      $operator: '>',
                      $field: 'levelCnt',
                      $value: 1
                    }
                  ]
                }
              }
            }
          })

          return { ms: Date.now() - d, amount: x.x.length }
        },
        { port }
      )
    )
  }

  d = Date.now()

  const results = await Promise.all(workers)

  results.forEach((v, i) => {
    // console.log(
    //   chalk.gray(
    //     `    worker #${i} {Get all desc using descendants in ${v[0]} ms`
    //   )
    // )
    // (larger then 1)
    t.is(v[0].amount, 32752)
    v[1].terminate()
  })

  console.log(
    chalk.gray(
      `    Get all desc using descendants x${workerAmount} in ${Date.now() -
        d} ms`
    )
  )

  await wait(1e3)

  await client.destroy()
})
