import test from 'ava'
import { moduleId as parentModuleId, connect, connections } from '../src'
import {
  startRegistry,
  startOrigin,
  startReplica,
  SelvaServer,
} from '../../server'
import { wait, worker, removeDump } from './assertions'
import { join } from 'path'
import fs from 'fs'
import exec from 'async-exec'
import getPort from 'get-port'

const dir = join(process.cwd(), 'tmp', 'connection-raw-test')

test.before(removeDump(dir))
test.after(removeDump(dir))

test.serial('connection / server orchestration', async (t) => {
  await wait(2e3)
  const port = await getPort()
  const registry = await startRegistry({ port })
  const client = connect({
    port,
  })

  const origin = await startOrigin({
    default: true,
    registry: { port },
    dir: join(dir, 'or'),
  })

  const snurfServer = await startOrigin({
    name: 'snurf',
    registry: { port },
    dir: join(dir, 'snurforigin'),
  })

  await client.redis.keys(
    {
      port,
      host: '0.0.0.0',
    },
    '*'
  )

  await client.redis.smembers(
    {
      port,
      host: '0.0.0.0',
    },
    'servers'
  )

  await wait(1e3)

  await client.redis.smembers(
    {
      port,
      host: '0.0.0.0',
    },
    'servers'
  )

  await client.redis.hset(
    {
      type: 'origin',
    },
    'flappie',
    'snurf',
    'snarx'
  )

  await client.redis.keys(
    {
      type: 'origin',
    },
    '*'
  )

  await client.redis.hset(
    {
      type: 'origin',
      name: 'snurf',
    },
    'snufels',
    'snurf',
    'snarx'
  )

  await client.redis.keys(
    {
      type: 'origin',
      name: 'snurf',
    },
    '*'
  )

  await snurfServer.destroy()

  const replicasPromises = [
    startReplica({
      registry: { port },
      default: true,
      dir: join(dir, 'replica1'),
    }),

    startReplica({
      registry: { port },
      default: true,
      dir: join(dir, 'replica2'),
    }),

    startReplica({
      registry: { port },
      default: true,
      dir: join(dir, 'replica3'),
    }),

    startReplica({
      registry: { port },
      default: true,
      dir: join(dir, 'replica4'),
    }),
  ]

  await client.redis.keys(
    {
      type: 'replica',
    },
    '*'
  )

  const oneReplica = await client.getServer(
    { type: 'replica' },
    { strict: true }
  )

  try {
    await client.updateSchema({
      rootType: {
        fields: {
          flap: { type: 'string' },
        },
      },
    })

    console.info('schema correct')
  } catch (err) {
    console.error(err)
  }

  await client.set({
    $id: 'root',
    flap: 'flurpypants',
  })

  const putUnderLoad = async (r) => {
    worker(
      async ({ connect }, { r, port }) => {
        const client = connect({ port })
        const fn = async (r, cnt = 0) => {
          const q = []
          // has to depend a bit on the computer
          for (let i = 0; i < 1e5; i++) {
            q.push(
              client.redis.hgetall(r, ~~(1000 * Math.random()).toString(16))
            )
          }
          await Promise.all(q)
          if (cnt < 30) {
            fn(r, ++cnt)
          } else {
            await client.destroy()
            console.info('Done with load (30 x 100k)')
          }
        }
        fn(r)
      },
      { r, port }
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

  const [{ replica, moduleId, size }, w1] = await worker(
    async ({ connect, wait, moduleId, connections }, { port }) => {
      const client = connect({ port })
      const replica = await client.getServer({ type: 'replica' })
      await client.redis.hgetall(replica, 'flappie')
      await wait(3100)
      await client.redis.hset({ type: 'origin' }, 'hurk', 'snef', 'schlurhp')
      await wait(3100)
      return { replica, moduleId, size: connections.size }
    },
    { port }
  )

  w1.terminate()

  // put under load test combined with disconnect
  // emulting busy errors
  // just to make sure
  t.true(moduleId !== parentModuleId, 'worker runs in different context')

  t.is(size, 1, 'removed the replica and origin connections')

  t.true(
    secondReplica.port !== replica.port,
    'When the second replica is under load, other replica becomes prefered'
  )

  const client2 = connect({ port })

  client2.redis.on({ type: 'replica' }, 'message', () => {})
  client2.redis.subscribe({ type: 'replica' }, 'snurf')

  await wait(50)

  const cId = client2.selvaId
  await client2.destroy()

  connections.forEach((c) => {
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
      snuxResults.push(msg)
    }
  })

  client.redis.subscribe({ type: 'replica' }, 'snux')

  const p = []
  for (let i = 0; i < 10e3; i++) {
    p.push(client.redis.hgetall({ type: 'replica' }, 'flappie'))
  }

  await wait(1e3)

  const snuxResults = []

  const [, w2] = await worker(
    async ({ connect, wait, port }) => {
      console.info('connect')
      const client = connect({ port })
      client.redis.on(
        { type: 'replica', strict: true },
        'message',
        (channel, msg) => {
          if (channel === 'snux') {
            // and count these!
            console.info('something from oneReplica', msg)
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

      await 1e3
    },
    { port }
  )

  w2.terminate()

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
  for (let i = 0; i < 10e3; i++) {
    x.push({ snurf: 'snarx' })
  }

  t.deepEqualIgnoreOrder(
    stateReconnectedSets,
    x,
    'handled all gets from a reconnection state (20k)'
  )

  await registry.destroy()
  await origin.destroy()
  await client.destroy()

  // can happen that registyr is closed before the other servers and it needs to wait for a hdc
  await t.connectionsAreEmpty()
})

test.serial('Get server raw - heavy load', async (t) => {
  const port = await getPort()
  const registry = await startRegistry({ port })
  const origin = await startOrigin({ registry: { port }, default: true })
  const client = connect({ port })
  const p = []
  const d = Date.now()
  const amount = 50e3
  const compare = []
  for (let i = 0; i < 20; i++) {
    compare.push({
      money: String(amount * 3 - 1),
    })
    p.push(
      worker(
        async ({ connect }, { port, amount }) => {
          const client = connect({ port })
          const makeitrain = async (index) => {
            const p = []
            for (let i = 0; i < amount; i++) {
              p.push(
                client.redis.hset(
                  { type: 'origin' },
                  'flax-' + client.uuid,
                  'money',
                  i + index * amount
                )
              )
            }
            await Promise.all(p)
          }
          for (let i = 0; i < 3; i++) {
            await makeitrain(i)
          }
        },
        { amount, port }
      )
    )
  }
  ;(await Promise.all(p)).map(([, w]) => w.terminate())
  const keys = await client.redis.keys({ type: 'origin' }, '*')
  const results = await Promise.all(
    keys
      .filter((k: string) => k.indexOf('flax-') === 0)
      .map((k: string) => client.redis.hgetall({ type: 'origin' }, k))
  )
  const total = amount * 3 * 20
  console.info('Executed', total / 1e3, 'k hsets', 'in', Date.now() - d, 'ms')
  t.deepEqualIgnoreOrder(
    results,
    compare,
    `used workers to set all fields correctly (${total} sets)`
  )
  await registry.destroy()
  await origin.destroy()
  await client.destroy()

  // when an uncertain state error happens it will take longer to clean

  await t.connectionsAreEmpty()
})

test.serial('registry reconnect', async (t) => {
  const port = await getPort()

  let registry = await startRegistry({ port })

  let current = port

  const connectOpts = async () => {
    return {
      port: current,
      host: '0.0.0.0',
    }
  }

  const origin = await startOrigin({ registry: connectOpts, default: true })

  const client = connect(connectOpts)

  await client.redis.hset({ type: 'origin' }, 'snurk', 'x', 1)

  await wait(5)

  current = await getPort()

  await registry.destroy()

  await wait(4500)

  registry = await startRegistry({ port: current })

  // promise on connected is maybe nice

  const x = await client.redis.hget({ type: 'origin' }, 'snurk', 'x')

  t.is(x * 1, 1, 'get back value after destroy')

  await wait(1500)

  await registry.destroy()
  await origin.destroy()
  await client.destroy()

  await t.connectionsAreEmpty()
})

test.serial('connection failure', async (t) => {
  const port = await getPort()

  const registry = await startRegistry({ port })

  const connectOpts = { port }

  const origin = await startOrigin({ registry: connectOpts, default: true })

  origin.on('error', () => {})

  let timeoutCnt = 0

  registry.on('server-timeout', (s) => {
    timeoutCnt++
  })

  const lua = fs
    .readFileSync(join(__dirname, './assertions/heavyLoad.lua'))
    .toString()

  const client = connect({ port })

  client.redis.subscribe({ type: 'origin' }, 'log')
  client.redis.on({ type: 'origin' }, 'message', (c, msg) => {
    console.info(msg)
  })

  const r = await client.redis.eval({ type: 'origin' }, lua, 0)

  t.is(timeoutCnt, 1, 'origin timed out once')

  t.is(r, 'x', 'correct return after heavy script / busy errors')

  await wait(5e3)

  await registry.destroy()
  await origin.destroy()
  await client.destroy()

  // takes longer because it needs to wait for a hard dc for the origin (a load script command is still in the queue)
  await t.connectionsAreEmpty()
})

test.serial(
  'Forcefully destroy redis server (and hope for restart)',
  async (t) => {
    const port = await getPort()
    const registry = await startRegistry({ port })
    const connectOpts = { port }
    const origin = await startOrigin({
      registry: connectOpts,
      default: true,
    })
    let timeoutCnt = 0
    origin.on('error', () => {
      // redis crash
      timeoutCnt++
    })
    const client = connect({ port })
    await wait(100)
    console.info('kill server')
    await exec(`kill -9 ${origin.pm.pid}`)
    await client.redis.set({ type: 'origin' }, 'x', 'bla')
    const x = await client.redis.get({ type: 'origin' }, 'x')
    t.is(x, 'bla')
    t.is(timeoutCnt, 1, 'origin timed out once')
    await wait(100)
    await registry.destroy()
    await origin.destroy()
    await client.destroy()
    // takes longer because it needs to wait for a hard dc for the origin (a load script command is still in the queue)
    await t.connectionsAreEmpty()
  }
)

test.serial('Change origin and re-conn replica', async (t) => {
  const port = await getPort()
  const registry = await startRegistry({ port })
  const connectOpts = { port }
  let origin = await startOrigin({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'replicarestarterorigin'),
  })

  const replica = await startReplica({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'replicarestarter'),
  })

  replica.on('error', (err) => {
    console.error(err)
  })

  const client = connect({ port })

  await client.redis.set({ type: 'origin' }, 'f', 'snurf')

  const x = await client.redis.get({ type: 'replica', strict: true }, 'f')

  t.is(x, 'snurf')

  await wait(100)

  await origin.destroy()

  await wait(5e3)

  origin = await startOrigin({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'replicarestarterorigin'),
  })

  const y2 = await client.redis.get({ type: 'origin' }, 'f')

  t.is(y2, 'snurf', 'get snurf from backup on origin')

  // wait till done
  const y = await client.redis.get({ type: 'replica', strict: true }, 'f')

  // may need to remove dump
  t.is(y, 'snurf', 'get snurf from re-connected replica')

  await wait(6000)
  await replica.destroy()
  await registry.destroy()
  await origin.destroy()
  await client.destroy()
  await t.connectionsAreEmpty()
})
