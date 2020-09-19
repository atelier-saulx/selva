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

  // need to test lesst strict we just want these numbers
  const resultSpread = [1, 2, 1]

  const s = await getServersSubscriptions()
  console.log('???????', s)

  t.deepEqualIgnoreOrder(
    Object.values(await getServersSubscriptions()).map(v => v.length),
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

      await wait(1e3)
      await client.destroy()
      await wait(1e3)
    },
    { port }
  )

  t.deepEqualIgnoreOrder(
    Object.values(await getServersSubscriptions()).map(v => v.length),
    resultSpread,
    'Correct spread after worker tries the same subscriptions'
  )

  await wait(1e3)
  obs.unsubscribe()
  obs2.unsubscribe()
  obs3.unsubscribe()
  obs4.unsubscribe()

  await wait(2000)

  t.deepEqual(
    await getServersSubscriptions(),
    {},
    'all subs are removed form subsregistry'
  )

  const [, w] = await worker(
    async ({ connect, wait }, { port }) => {
      const client = connect({ port })
      const obs = client.observe({
        $id: 'root',
        value: true
      })
      obs.subscribe(() => {})
      console.log('UUID FOR WORKER', obs.clientUuid)
      await wait(1e3)
    },
    { port }
  )
  w.terminate()

  t.deepEqualIgnoreOrder(
    Object.values(await getServersSubscriptions()).map(v => v.length),
    [1],
    'New sub is added'
  )

  const servers2 = await getServersSubscriptions()

  console.log(servers2)

  // make a thign to check that it auto clears

  await wait(30e3)

  t.deepEqual(
    await getServersSubscriptions(),
    {},
    'all subs are removed form subsregistry'
  )

  await wait(500)
  await client.destroy()
  await subsmanager.destroy()
  await subsmanager2.destroy()
  await subsmanager3.destroy()
  await subsregistry.destroy()
  await registry.destroy()
  await origin.destroy()
  await t.connectionsAreEmpty()
})
