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

// const x= '/dahsbo/:id/:smurk'.replace(/:s*.*?(?=\s*\/)/g, 'lur')

// in: '/dahsbo/:id/:smurk' out: '/dahsbo/lur/lur'

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

  await wait(100)

  const obs3 = client.observe({
    $id: 'root',
    nest: true,
    value: true
  })

  obs3.subscribe(() => {})

  client
    .getServer(
      {
        type: 'subscriptionManager'
      },
      { subscription: obs.uuid }
    )
    .then(v => {
      console.log('--->', v)
    })

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

  await wait(1e3)

  console.log('XXXXXXXXXXXXXXXXXXXXXXXXXXXX')

  console.log(await getServersSubscriptions())

  console.log(client.servers.subsManagers)

  await worker(
    async ({ connect, wait }, { port }) => {
      const client = connect({ port })
      client
        .observe({
          $id: 'root',
          value: true
        })
        .subscribe((value, checksum, diff) => {
          console.log('yesh2', value, checksum, diff)
        })

      await wait(3e3)
      client.destroy()
    },
    { port }
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
