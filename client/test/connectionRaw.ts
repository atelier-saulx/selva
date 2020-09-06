import test from 'ava'
import { moduleId as parentModuleId, connect, connections } from '@saulx/selva'
import {
  startRegistry,
  startOrigin,
  startReplica,
  SelvaServer
} from '../../server'
import './assertions'
import { wait, worker } from './assertions'
import { join } from 'path'

const dir = join(process.cwd(), 'tmp', 'connection-raw-test')

test.serial('connection / server orchestration', async t => {
  startRegistry({ port: 9999 })

  const client = connect({
    port: 9999
  })

  // add these events
  client.registryConnection.on('connect', () => {
    console.log('ok connect registry client')
  })

  client.registryConnection.on('hard-disconnect', () => {
    console.log('hard dc on registry')
  })

  client.registryConnection.on('disconnect', () => {
    console.log('ok dc registry client')
  })

  client.registryConnection.on('close', () => {
    console.log('destroy registry connection')
  })

  startOrigin({
    default: true,
    registry: { port: 9999 }
  })

  const snurfServer = await startOrigin({
    name: 'snurf',
    registry: { port: 9999 },
    dir: join(dir, 'origin')
  })

  await client.redis.keys(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    '*'
  )

  await client.redis.smembers(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    'servers'
  )

  await wait(1e3)

  await client.redis.smembers(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    'servers'
  )

  await client.redis.hset(
    {
      type: 'origin'
    },
    'flappie',
    'snurf',
    'snarx'
  )

  await client.redis.keys(
    {
      type: 'origin'
    },
    '*'
  )

  await client.redis.hset(
    {
      type: 'origin',
      name: 'snurf'
    },
    'snufels',
    'snurf',
    'snarx'
  )

  await client.redis.keys(
    {
      type: 'origin',
      name: 'snurf'
    },
    '*'
  )

  await snurfServer.destroy()

  const replicasPromises = [
    startReplica({
      registry: { port: 9999 },
      default: true,
      dir: join(dir, 'replica1')
    }),

    startReplica({
      registry: { port: 9999 },
      default: true,
      dir: join(dir, 'replica2')
    }),

    startReplica({
      registry: { port: 9999 },
      default: true,
      dir: join(dir, 'replica3')
    }),

    startReplica({
      registry: { port: 9999 },
      default: true,
      dir: join(dir, 'replica4')
    }),

    await client.redis.keys(
      {
        type: 'replica'
      },
      '*'
    )
  ]

  const oneReplica = await client.getServer(
    { type: 'replica' },
    { strict: true }
  )

  const putUnderLoad = async r => {
    worker(
      async ({ connect }, { r }) => {
        const client = connect({ port: 9999 })
        const fn = async (r, cnt = 0) => {
          let q = []
          // has to depend a bit on the computer
          for (let i = 0; i < 100000; i++) {
            q.push(
              client.redis.hgetall(r, ~~(1000 * Math.random()).toString(16))
            )
          }
          await Promise.all(q)
          if (cnt < 30) {
            fn(r, ++cnt)
          } else {
            console.log('Done with load (50 x 100k)')
          }
        }
        fn(r)
      },
      { r }
    )
    await wait(500)
  }

  await putUnderLoad(oneReplica)

  // now getting a replica needs to get another one
  const secondReplica = await client.getServer(
    { type: 'replica' },
    { strict: true }
  )

  t.true(
    secondReplica.port !== oneReplica.port,
    'When the first replica is under load, other replica becomes prefered'
  )

  await putUnderLoad(secondReplica)

  const [{ replica, moduleId, size }] = await worker(
    async ({ connect, wait, moduleId, connections }) => {
      const client = connect({ port: 9999 })
      const replica = await client.getServer({ type: 'replica' })
      await client.redis.hgetall(replica, 'flappie')
      await wait(3100)
      await client.redis.hset({ type: 'origin' }, 'hurk', 'snef', 'schlurhp')
      await wait(3100)
      return { replica, moduleId, size: connections.size }
    }
  )

  // put under load test combined with disconnect
  // emulting busy errors
  // just to make sure
  t.true(moduleId !== parentModuleId, 'worker runs in different context')

  t.is(size, 1, 'removed the replica and origin connections')

  t.true(
    secondReplica.port !== replica.port,
    'When the second replica is under load, other replica becomes prefered'
  )

  const client2 = connect({ port: 9999 })

  client2.redis.on({ type: 'replica' }, 'message', () => { })
  client2.redis.subscribe({ type: 'replica' }, 'snurf')

  await wait(50)

  console.log('DESTROY 2nd client!')
  const cId = client2.selvaId
  client2.destroy()

  connections.forEach(c => {
    t.true(c.getConnectionState(cId).isEmpty, 'connection is empty')
  })

  await wait(5e3)
  const r = await Promise.all(replicasPromises)

  r.forEach((server: SelvaServer) => {
    if (server.destroy) {
      server.destroy()
    }
  })

  client.redis.on({ type: 'replica' }, 'message', (channel, msg) => {
    if (channel === 'snux') {
      // we are going to count these
      // console.log('On the original something from oneReplica', msg)
      snuxResults.push(msg)
    }
  })

  client.redis.subscribe({ type: 'replica' }, 'snux')

  const p = []
  for (let i = 0; i < 20e3; i++) {
    p.push(client.redis.hgetall(
      { type: 'replica' },
      'flappie'
    ))
  }

  await wait(1e3)

  const snuxResults = []

  const [] = await worker(async ({ connect, wait }) => {
    console.log('connect')
    const client = connect({ port: 9999 })

    client.redis.on(
      { type: 'replica', strict: true },
      'message',
      (channel, msg) => {
        if (channel === 'snux') {
          // and count these!
          console.log('something from oneReplica', msg)
        }
      }
    )

    client.redis.subscribe({ type: 'replica', strict: true }, 'snux')
    client.redis.publish(
      { type: 'replica', strict: true },
      'snux',
      'flurpy pants swaffi'
    )
    client.redis.publish(
      { type: 'replica', strict: true },
      'snux',
      'flurpy pants 1'
    )

    await wait(30)

    // no we want to get hard dc and have this in the queue in progress
    for (let i = 0; i < 5; i++) {
      client.redis.publish(
        { type: 'replica', strict: true },
        'snux',
        'flurpy pants -- looper - ' + i
      )
    }

    return
  })
  for (let i = 0; i < 20e3; i++) {
    client.redis.publish(
      { type: 'replica' },
      'snux',
      'flurpy pants -- looper - ' + i
    )
  }

  await wait(5000)

  // server exemption

  // maybe put this in a worker - better for testing if it actualy arrives
  await wait(1500)

  // need to add ramp up!!!
  t.is(snuxResults.length, 20e3 + 4, 'Resend all events on hard disconnect')


  const stateReconnectedSets = await Promise.all(p)

  const x = []
  for (let i = 0; i < 20e3; i++) {
    x.push({ snurf: 'snarx' })
  }

  t.deepEqualIgnoreOrder(stateReconnectedSets, x, 'handled all gets from a reconnection state (20k)')
})
