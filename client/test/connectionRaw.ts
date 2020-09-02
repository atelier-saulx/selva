import test from 'ava'
import { Connection, connections, connect } from '@saulx/selva'
import { startRegistry, startOrigin, startReplica } from '../../server'
import './assertions'
import { wait } from './assertions'
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

  startReplica({
    registry: { port: 9999 },
    default: true,
    dir: join(dir, 'replica1')
  })

  startReplica({
    registry: { port: 9999 },
    default: true,
    dir: join(dir, 'replica2')
  })

  startReplica({
    registry: { port: 9999 },
    default: true,
    dir: join(dir, 'replica3')
  })

  startReplica({
    registry: { port: 9999 },
    default: true,
    dir: join(dir, 'replica4')
  })

  await client.redis.keys(
    {
      type: 'replica'
    },
    '*'
  )

  const oneReplica = await client.getServer(
    { type: 'replica' },
    { strict: true }
  )

  const putUnderLoad = async (r, cnt = 0) => {
    let q = []
    for (let i = 0; i < 10000; i++) {
      q.push(client.redis.hgetall(r, ~~(1000 * Math.random()).toString(16)))
    }
    await Promise.all(q)
    if (cnt < 50) {
      putUnderLoad(r, ++cnt)
    } else {
      console.log('done with load (100 x 10k)', r)
    }
  }

  putUnderLoad(oneReplica)

  await wait(1e3)

  // now getting a replica needs to get another one
  const secondReplica = await client.getServer(
    { type: 'replica' },
    { strict: true }
  )

  t.true(
    secondReplica.port !== oneReplica.port,
    'When the first replica is under load, other replica becomes prefered'
  )

  // selva client emit reconnect event (with descriptor)
})
