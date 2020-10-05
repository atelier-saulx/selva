import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
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
  const levels = 10
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

  d = Date.now()
  const ultraResults = await client.get(myQuery)
  console.log(
    chalk.gray(
      `    Get ${amount}^${levels} things using nested queries in ${Date.now() -
        d} ms`
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
    `has correct amount of result (${levelMap[levels - 1]}) for ${levels} deep`
  )

  await wait(1e3)

  await client.destroy()
})
