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
  console.log('go make')
  registry = (await start()).registry
  console.log('its start')
})

test.after(async _t => {
  await stop()
})

test.serial('Perf - Simple increment', async t => {
  const result = await run(
    async (client, index) => {
      const p = []

      for (let i = 0; i < 1000; i++) {
        p.push(
          client.set({
            $id: 'root',
            value: i
          })
        )
      }

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

test.skip('Perf - Subscriptions', async t => {
  const result = await run(
    async client => {
      const sub = client
        .observe({
          $id: 'root',
          value: true,
          flaperdrol: {
            $db: 'other',
            $id: 'root',
            value: true
          }
        })
        .subscribe(x => {
          // sub.unsubscribe()
          setTimeout(() => sub.unsubscribe(), ~~(Math.random() * 50))
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
