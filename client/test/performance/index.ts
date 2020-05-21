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

      for (let i = 0; i < 1000; i++) {
        x.push('yabbadabba:yabbadabba')
        x.push('yabbadabbayabbadabbayabbadabba')
        x.push(JSON.stringify({ flap: 'x' }))
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
          `local i = 0
          local j = 0

          local result = {}
          while i < #ARGV do
            cjson.decode(ARGV[i + 3])
            redis.call("hset", "x", "y", 1)
            redis.call("publish", "x", "y")

            result[j] = "yabbadabba"

            do
              i = i + 3
              j = j + 1
            end
          end
          return cjson.encode(result)`,
          0,
          ...x
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
