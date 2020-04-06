import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'
import vm from 'vm'
import m from 'module'

let srv
let port: number
let vms

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      team: {
        prefix: 'te',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('perf - Set multiple using 5 clients', async t => {
  //@ts-ignore
  const total = (global.total = {})

  const code = function(i, port) {
    console.log(`Start vm ${i}`)
    const { connect } = require('../src/index')
    const client = connect({ port })

    let iteration = 1
    let time = 0
    let amount = 10000
    const setLoop = async () => {
      // @ts-ignore
      if (global.stopped) {
        console.log('stop client', i)
      } else {
        const q = []
        for (let i = 0; i < amount; i++) {
          q.push(
            client.set({
              type: 'match',
              value: ~~(Math.random() * 1000)
            })
          )
        }
        let d = Date.now()
        await Promise.all(q)
        time += Date.now() - d

        //   console.log(
        //     `Client ${i} iteration ${iteration} finished in ${Date.now() -
        //       d}ms total set ${iteration * amount}`
        //   )

        iteration++
        //@ts-ignore
        global.total[i] = { amount: iteration * amount, time }

        setLoop()
      }
    }

    client.on('connect', () => {
      console.log(`Connected ${i}`)
      setLoop()
    })
  }

  const vms = []

  const clientAmount = 5

  for (let i = 0; i < clientAmount; i++) {
    let wrappedRequire = require
    vms.push(
      vm.runInThisContext(m.wrap(`(${code.toString()})(${i},${port})`), {
        filename: 'aristotle-vm.js'
      })(
        exports,
        wrappedRequire,
        module,
        __filename, // and correct filename
        __dirname // put correct dirname in there
      )
    )
  }

  let it = 0

  const getTotal = () => {
    let a = 0
    for (let key in total) {
      a += total[key].amount
    }
    return a
  }

  const time = 1

  const int = setInterval(() => {
    it++
    if (it - 2 > 0) {
      const s = time * (it - 2)

      console.log(
        `${Math.round(
          getTotal() / s
        )}/s Processed ${getTotal()} items in ${s}s using ${clientAmount} clients`
      )
    }
  }, time * 1e3)

  const client = connect({ port })

  const s = await client.observe({
    items: {
      value: true,
      id: true,
      $list: {
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'value',
            $operator: '=',
            $value: 10
          }
        }
      }
    }
  })

  s.subscribe(d => {
    console.log('hey update', d)
  })

  //   const client = connect({ port })
  //   const sub = await client.observe({
  //     items: {
  //       $list: {
  //         $find: {
  //           $traverse: 'descendants',
  //           $filter: {
  //             $field: 'value',
  //             $operator: '>',
  //             $value: 10
  //           }
  //         }
  //       }
  //     }
  //   })

  //   let cnt = 0
  //   sub.subscribe(d => {
  //     console.log('incoming', d)
  //     cnt++
  //   })

  await wait(6e4 * 10)
  clearInterval(int)
  // @ts-ignore
  global.stopped = true
  await wait(1e3)
  t.true(getTotal() > 20e3)
})
