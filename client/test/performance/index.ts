import test from 'ava'
import { connect } from '../../src/index'
import {
  startRegistry,
  startOrigin,
  startSubscriptionManager,
  startReplica
} from '@saulx/selva-server'
import { start, stop, run } from './testRunner'

let registry

test.before(async t => {
  registry = (await start()).registry
})

test.after(async _t => {
  await stop()
})

test.skip('Perf - Simple increment', async t => {
  const result = await run(
    async client => {
      const x = []
      const p = []

      for (let i = 0; i < 1e4; i++) {
        // x.push('yabbadabba:yabbadabba')
        // x.push('yabbadabbayabbadabbayabbadabba')
        x.push(JSON.stringify({ flap: 'x' }))
        // x.push({ flap: 'x' })
      }
      //@ts-ignore
      if (!global.isEVALSHA) {
        p.push(
          client.redis.command(
            'script',
            'load',
            `local i = 0
          local j = 0

          local result = {}
          while i < #ARGV do
            cjson.decode(ARGV[i + 1])
            redis.call("hset", "x", "y", 1)
            redis.call("publish", "x", "y")

            result[j + 1] = "yabbadabba"

            do
              i = i + 1
              j = j + 1
            end
          end
          return cjson.encode(result)`
          )
        )

        const y = await Promise.all(p)
        console.log(y)
        //@ts-ignore
        global.isEVALSHA = y[0]
      } else {
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

        // cjson add arround 10ms
        //@ts-ignore

        // for (let i = 0; i < 1e3; i++) {
        // p.push(
        //   client.redis.eval(
        //     `local i = 0
        // local j = 0

        // local result = {}
        // local x = cjson.decode('${JSON.stringify(x)}')
        // while i < #x do
        //   redis.call("hset", "x", "y", 1)
        //   redis.call("publish", "x", "y")
        //   result[j + 1] = "yabbadabba"
        //   do
        //     i = i + 1
        //     j = j + 1
        //   end
        // end
        // return cjson.encode(result)`,
        //     0
        //   )
        // )
        // }

        // for (let i = 0; i < 1e4; i++) {

        // make nice!
        // 1800 -> 200 -- 9x
        p.push(client.redis.command('selva.flurpypants'))
        // }

        // for (let i = 0; i < 1e3; i++) {
        //   //@ts-ignore
        // p.push(client.redis.evalsha(global.isEVALSHA, 0, ...x))
        // }

        await Promise.all(p)
      }
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

// test.skip('Perf - Simple increment and adding meta', async t => {
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

test.serial('Perf - Subscriptions', async t => {
  const result = await run(
    async client => {
      const sub = client
        .observe({
          $id: 'root',
          value: true
        })
        .subscribe(x => {
          // sub.unsubscribe()
          setTimeout(() => sub.unsubscribe(), ~~(Math.random() * 100))
        })

      await client.set({
        $id: 'root',
        value: Math.floor(Date.now() / 1e3),
        children: [
          {
            $id: 'th1',
            type: 'thing',
            value: 1
          }
        ]
      })

      //@ts-ignore
      await wait(100)
    },
    {
      label: 'Observe',
      clients: 100,
      time: 100e3
    }
  )

  t.true(result.iterations > 1e6)
})
