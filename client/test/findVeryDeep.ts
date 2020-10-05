import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'
import set from '../src/set/fieldParsers/set'
import { level } from 'chalk'

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
  const levels = 6

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
      for (let j = 0; j < 2; j++) {
        ++levelMap[myLevel]
        let n: any = {
          type: 'glurp',
          title: `Level ${myLevel} child ${j} level count -> ${levelMap[myLevel]}`
        }
        x.children.push(n)
        recurse(n, nextI)
      }
    }
  }
  recurse(setObj)

  console.dir(setObj, { depth: 100 })

  setObj.$id = 'root'

  var d = Date.now()
  await client.set(setObj)

  console.log('ok make it nice', Date.now() - d, 'ms')

  const myQuery = {
    x: {
      title: true,
      id: true,
      $list: {
        $find: q.$find
      }
    }
  }
  console.dir(myQuery, { depth: 100 })

  const ultraResults = await client.get(myQuery)

  console.dir(ultraResults, { depth: 10 })

  await wait(1e3)

  await client.destroy()
})
