import test from 'ava'
import { connect } from '../../src/index'
import {
  startRegistry,
  startOrigin,
  startSubscriptionManager,
  startReplica
} from '@saulx/selva-server'
import { wait } from '../assertions'
import { start, stop, run } from './testRunner'

let registry

test.before(async t => {
  registry = (await start()).registry
})

test.after(async _t => {
  await stop()
})

test.serial('Perf - Simple increment', async t => {
  const result = await run(
    async client => {
      const x = []
      const p = []

      let tmp = []

      for (let i = 0; i < 10000; i++) {
        x.push({ flap: 'x' })
        tmp.push('redis.call("hset", "x", "y", 1)')
        tmp.push('redis.call("publish", "x", "y")')
      }

      // json parse?
      // stringify it

      // for (let i = 0; i < 1000; i++) {
      //   // p.push(
      //   //   client.set({
      //   //     $id: 'root',
      //   //     value: i
      //   //   })
      //   // )
      //   // p.push(client.redis.set('flurp', i))
      //   p.push(client.redis.eval('return redis.call("hset", "x", "y", 1)', 0))
      // }

      p.push(
        client.redis.eval(
          `${tmp.join('\n')}
          cjson.decode('${JSON.stringify(x)}')
           return 1`,
          0
        )
      )

      await Promise.all(p)
    },
    {
      label: 'Simple increment',
      clients: 10,
      time: 10e3
    }
  )

  const client = connect({ port: registry.port })

  console.log(await client.get({ $id: 'root', value: true }))

  console.log(result)

  t.true(result.iterations > 1e6)
})

// test.serial('Perf - Simple increment and adding meta', async t => {
//   const result = await run(
//     async client => {
//       await client.set({
//         $id: 'root',
//         value: { $increment: 1 },
//         children: [
//           {
//             type: 'thing',
//             value: 1
//           }
//         ]
//       })
//     },
//     {
//       label: 'Simple increment add meta',
//       clients: 10,
//       time: 2e3
//     }
//   )
//   t.true(result.iterations > 1e6)
// })
