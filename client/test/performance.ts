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
let portSubs: number

test.before(async t => {
  port = await getPort()
  portSubs = await getPort()
  // small test
  srv = await start({
    port: new Promise(r => {
      setTimeout(() => r(port), 100)
    }),
    subscriptions: {
      port: new Promise(r => {
        setTimeout(() => r(portSubs), 100)
      })
    }
  })

  const client = connect({ port, subscriptions: { port: portSubs } })
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

test.serial('perf - Set a lot of things', async t => {
  //@ts-ignore
  const total = (global.total = {})

  const code = function(i, port, portSubs) {
    console.log(`Start vm ${i}`)
    const { connect } = require('../src/index')
    const client = connect({
      port,
      subscriptions: {
        port: portSubs
      }
    })

    client.on('connect', () => {
      console.log('connect main client!')
    })

    let iteration = 1
    let time = 0
    let amount = 1500
    const setLoop = async () => {
      // @ts-ignore
      if (global.stopped) {
        console.log('stop client', i)
      } else {
        const q = []
        // for (let i = 0; i < amount; i++) {
        //   q.push(
        //     client.set({
        //       type: 'match',
        //       value: ~~(Math.random() * 1000)
        //     })
        //   )
        // }

        for (let i = 0; i < amount; i++) {
          q.push({
            type: 'match',
            value: ~~(Math.random() * 1000)
          })
        }

        await client.set({
          $id: 'root',
          children: q //{ $add: q }
          // children: { $add: q }
        })

        // await Promise.all(q)
        time += 1

        iteration++

        //@ts-ignore
        global.total[i] = { amount: iteration * amount, time }

        // await wait(1e3)

        // if sets are larger then a certain amount do a wait on the client to let it gc a bit
        // for gc ?

        // setTimeout(setLoop, 1e3)
        setLoop()
      }
    }

    client.on('connect', () => {
      console.log(`Connected ${i}`)
      setTimeout(setLoop, 1e3)
    })
  }

  const vms = []

  const clientAmount = 2

  for (let i = 0; i < clientAmount; i++) {
    let wrappedRequire = require
    vms.push(
      vm.runInThisContext(
        m.wrap(`(${code.toString()})(${i},${port}, ${portSubs})`),
        {
          filename: `client-vm${i}.js`
        }
      )(
        exports,
        wrappedRequire,
        module,
        __filename, // and correct filename
        __dirname // put correct dirname in there
      )
    )
  }

  const getTotal = () => {
    let a = 0
    for (let key in total) {
      a += total[key].amount
    }
    return a
  }

  const getTotalTime = () => {
    let t = 0
    for (let key in total) {
      t += total[key].time
    }
    return 0
  }

  const time = Date.now()

  const int = setInterval(() => {
    const s = Math.round((Date.now() - time) / 1000) - getTotalTime()

    console.log(
      `${Math.round(
        getTotal() / s
      )}/s Processed ${getTotal()} items in ${s}s using ${clientAmount} clients`
    )
  }, 1e3)

  const client = connect({ port })

  const s = await client.observe({
    items: {
      value: true,
      id: true,
      $list: {
        $limit: 10,
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

  const s2 = await client.observe({
    items: {
      value: true,
      id: true,
      type: true,
      $list: {
        $limit: 10,
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'value',
            $operator: '>',
            $value: 1
          }
        }
      }
    }
  })

  s2.subscribe(d => {
    console.log('hey update 2', d)
  })

  await wait(10e3)
  clearInterval(int)
  // @ts-ignore
  global.stopped = true
  await wait(1e3)
  t.true(getTotal() > 40e3)
})
