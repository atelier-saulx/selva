import test from 'ava'
import { connect, connections, Observable, constants } from '@saulx/selva'
import {
  startRegistry,
  startOrigin,
  startReplica,
  startSubscriptionManager,
  startSubscriptionRegistry
} from '../../server/dist'
import './assertions'
import { wait, worker, removeDump } from './assertions'
import { join } from 'path'
import getPort from 'get-port'

const dir = join(process.cwd(), 'tmp', 'observable-raw-test')

test.before(removeDump(dir))
test.after(removeDump(dir))

test.serial('Make some observables', async t => {
  // maybe run all the servers in workers
  const port = await getPort()
  const registry = await startRegistry({ port })
  const connectOpts = { port }

  const origin = await startOrigin({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'observablesgotime')
  })

  // one extra to offload registry
  const subsregistry = await startSubscriptionRegistry({
    registry: connectOpts
  })

  // do this later startReplica
  const subsmanager = await startSubscriptionManager({
    registry: connectOpts
  })

  const subsmanager2 = await startSubscriptionManager({
    registry: connectOpts
  })

  const subsmanager3 = await startSubscriptionManager({
    registry: connectOpts
  })

  const client = connect({ port })

  const getServersSubscriptions = async (): Promise<{
    [server: string]: string[]
  }> => {
    const servers = await client.redis.keys(
      { type: 'subscriptionRegistry' },
      constants.REGISTRY_SUBSCRIPTION_INDEX + '*'
    )
    const s = {}
    for (let k of servers) {
      const x = await client.redis.smembers({ type: 'subscriptionRegistry' }, k)
      s[k.replace(constants.REGISTRY_SUBSCRIPTION_INDEX, '')] = x
    }
    return s
  }

  await client.updateSchema({
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    value: 1
  })

  const x = await client.get({ $id: 'root', value: true })

  t.deepEqual(x, { value: 1 })

  const obs = client.observe({
    $id: 'root',
    value: true
  })

  const obs2 = client.observe({
    $id: 'root',
    nest: true
  })

  obs2.subscribe(() => {})

  await wait(1200)

  const obs3 = client.observe({
    $id: 'root',
    nest: true,
    value: true
  })

  obs3.subscribe(() => {})

  obs.subscribe((value, checksum, diff) => {
    console.log('yesh', value, checksum, diff)
  })

  await wait(3e3)

  const obs4 = client.observe({
    $id: 'root',
    nest: {
      fun: true
    },
    value: true
  })

  obs4.subscribe(() => {})

  await wait(1200)

  const spread = await getServersSubscriptions()

  const v = Object.values(spread)

  const resultSpread = [
    ['706795659de96229b7166f18062a5d5d10f64b446187accc80a4679fd2c03a53'],
    [
      '63aef0b842066c8ff446a0f53b90c0b54120e7c100573fb5dfd31434b029993c',
      '85ea0edb9f8024a57df3f6b465c26eaeedc83d14f75b537fddbddf07f108cd17'
    ],
    ['c22547f0f450fd5d51635e157bb4f47559c815fb7c1eff01bc9cdee1c38540f2']
  ]

  t.deepEqualIgnoreOrder(
    v,
    resultSpread,
    'Correct spread of subscriptions on subs managers'
  )

  // use this worker to test
  //  - ordering of stuff
  //  - adds to the same subs manager
  await worker(
    async ({ connect, wait }, { port }) => {
      const client = connect({ port })
      client
        .observe({
          $id: 'root',
          value: true
        })
        .subscribe(() => {})

      client
        .observe({
          $id: 'root',
          nest: {
            fun: true
          },
          value: true
        })
        .subscribe(() => {})

      client
        .observe({
          $id: 'root',
          nest: true,
          value: true
        })
        .subscribe(() => {})

      await wait(3e3)
      client.destroy()
    },
    { port }
  )

  t.deepEqualIgnoreOrder(
    v,
    resultSpread,
    'Correct spread after worker tries the same subscriptions'
  )

  await wait(3e3)
  obs.unsubscribe()

  await wait(10e3)
  console.log('ok')

  // --------------------------------------------

  // subs registry is next step

  // then move subscription

  // --------------------------------------------

  // then diff

  // thene checks for connections destroy

  // --------------------------------------------

  t.pass()
})
