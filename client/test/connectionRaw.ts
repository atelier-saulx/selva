import test from 'ava'
import { moduleId as parentModuleId, connect } from '@saulx/selva'
import { startRegistry, startOrigin, startReplica } from '../../server'
import './assertions'
import { wait, worker } from './assertions'
import { join } from 'path'

const dir = join(process.cwd(), 'tmp', 'connection-raw-test')

test.serial(
  'connection / server orchestration (all on same process)',
  async t => {
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

    client.registryConnection.on('destroy', () => {
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
            if (cnt < 50) {
              fn(r, ++cnt)
            } else {
              console.log('done with load (50 x 100k)', r)
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
        // @ts-ignore
        global.flap = true
        console.log('connect')
        const client = connect({ port: 9999 })
        const replica = await client.getServer({ type: 'replica' })
        await client.redis.hgetall(replica, 'flappie')

        console.log(connections.keys())

        await wait(3100)

        // should destroy
        console.log(connections.keys())

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

    await wait(2e3)

    client.redis.on(oneReplica, 'message', (channel, msg) => {
      if (channel === 'snux') {
        console.log('something from oneReplica', msg)
      }
    })

    client.redis.subscribe(oneReplica, 'snux')

    await wait(500)

    // replica[0].

    const oneReplicaServer = (await Promise.all(replicasPromises)).find(
      server => server.port === oneReplica.port
    )

    console.log('destroy server')
    oneReplicaServer.destroy()

    await wait(10)

    // no we want to get hard dc and have this in the queue in progress
    client.redis.publish(oneReplica, 'snux', 'flurpy pants 2')

    await wait(3000)
  }
)
