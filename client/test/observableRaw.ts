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
import { ServerSelector } from '../dist/src/types'

const dir = join(process.cwd(), 'tmp', 'observable-raw-test')

test.before(removeDump(dir))
test.after(removeDump(dir))

test.serial('diff observables', async t => {
  const port = await getPort()
  const registry = await startRegistry({ port })
  const connectOpts = { port }

  const origin = await startOrigin({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'diff-origin')
  })

  // add in a replica

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

  const client = connect({ port })

  await client.updateSchema({
    rootType: {
      fields: {
        value: { type: 'number' },
        flurp: {
          type: 'array',
          items: { type: 'number' }
        },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' }
          }
        }
      }
    }
  })

  const obs = client.observe({
    $id: 'root',
    value: true,
    flurp: true
  })

  obs.subscribe((value, checksum, diff) => {
    // console.log('on subscription', value, checksum, diff)
  })

  await wait(500)

  await client.set({
    $id: 'root',
    value: 1
  })

  await wait(500)

  await client.set({
    $id: 'root',
    flurp: [1, 2, 3, 4]
  })

  await wait(500)

  obs.subscribe((value, checksum, diff) => {
    // console.log('on subscription 2', value, checksum, diff)
  })

  await wait(500)

  await client.set({
    $id: 'root',
    flurp: [1, 3, 2, 4]
  })

  client.subscribeSchema()

  await wait(2500)

  await client.destroy()
  await subsmanager.destroy()
  await subsmanager2.destroy()
  await subsregistry.destroy()
  await registry.destroy()
  await origin.destroy()
  await t.connectionsAreEmpty()
})

test.serial('Make some observables and many subs managers', async t => {
  // maybe run all the servers in workers
  const port = await getPort()
  const registry = await startRegistry({ port })
  const connectOpts = { port }

  const origin = await startOrigin({
    registry: connectOpts,
    default: true
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

  await wait(1e3)

  const obs3 = client.observe({
    $id: 'root',
    nest: true,
    value: true
  })

  obs3.subscribe(() => {})
  obs.subscribe(() => {})

  await wait(1e3)

  const obs4 = client.observe({
    $id: 'root',
    nest: {
      fun: true
    },
    value: true
  })

  obs4.subscribe(() => {})

  await wait(1e3)

  // need to test lesst strict we just want these numbers

  // schema subs as well!
  const resultSpread = [2, 2, 1]

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

  const servers = await getServersSubscriptions()

  const newClient = connect({ port })

  const obs5 = new Observable(
    {
      type: 'get',
      options: {
        $id: 'root',
        nest: {
          fun: true
        },
        value: true
      }
    },
    newClient
  )

  const uuid = obs5.uuid

  const serverSelector: ServerSelector = {
    type: 'subscriptionManager'
  }

  for (const id in servers) {
    const f = servers[id].find(u => u === uuid)
    if (!f) {
      const [host, port] = id.split(':')
      serverSelector.host = host
      serverSelector.port = Number(port)
      break
    }
  }

  const previousLocation = obs4.connection.serverDescriptor

  await obs5.start(serverSelector)

  await wait(5e3)

  const serversAfter = await getServersSubscriptions()

  t.deepEqual(
    obs4.connection.serverDescriptor,
    obs5.connection.serverDescriptor,
    'obs4 is moved to the same server as obs5'
  )

  const movedUuid = obs5.uuid
  const prevId = previousLocation.host + ':' + previousLocation.port
  const newId = serverSelector.host + ':' + serverSelector.port

  // console.log(prevId, newId)

  t.true(
    !serversAfter[prevId] || !serversAfter[prevId].includes(movedUuid),
    'Previous server does not have moved channel'
  )

  t.true(
    serversAfter[newId].includes(movedUuid),
    'New server does have moved channel'
  )

  t.is(
    Object.values(await getServersSubscriptions()).reduce((a, b) => {
      return a + b.length
    }, 0),
    5, // one for schema subs
    'after moving to obs5 still has 4 things'
  )

  // we just destroyed the other observable as well :/
  obs5.destroy()

  await wait(1e3)
  await newClient.destroy()

  t.is(
    Object.values(await getServersSubscriptions()).reduce((a, b) => {
      return a + b.length
    }, 0),
    5,
    'after removing obs5 still has 4 subs'
  )

  await wait(3e3)

  await subsmanager.destroy()
  await subsmanager2.destroy()

  await wait(3e3)

  t.is(
    Object.values(await getServersSubscriptions()).reduce((a, b) => {
      return a + b.length
    }, 0),
    5,
    'after removing servers still has 4 subs'
  )

  t.deepEqualIgnoreOrder(
    Object.values(await getServersSubscriptions()).map(v => v.length),
    [5],
    'Correct spread one 1 server is left'
  )

  await wait(10e3)

  obs.unsubscribe()
  obs2.unsubscribe()
  obs3.unsubscribe()
  obs4.unsubscribe()
  // send a publish of a subscription to a specific sub manager

  await wait(2000)

  t.deepEqual(
    Object.values(await getServersSubscriptions()).reduce((a, b) => {
      return a + b.length
    }, 0),
    1,
    'all subs are removed from subsregistry, except schema subs'
  )

  const [, w] = await worker(
    async ({ connect, wait }, { port }) => {
      const client = connect({ port })
      const obs = client.observe({
        $id: 'root',
        value: true
      })
      obs.subscribe(() => {})
      await wait(1e3)
    },
    { port }
  )

  t.deepEqualIgnoreOrder(
    Object.values(await getServersSubscriptions()).map(v => v.length),
    [2],
    'New sub is added'
  )

  w.terminate()

  await wait(5e3)

  t.deepEqual(
    Object.values(await getServersSubscriptions()).reduce((a, b) => {
      return a + b.length
    }, 0),
    1,
    'all subs are removed from subsregistry after worker is terminated, except schema'
  )

  await wait(500)
  await client.destroy()
  await subsmanager3.destroy()
  await subsregistry.destroy()
  await registry.destroy()
  await origin.destroy()
  await t.connectionsAreEmpty()
})
