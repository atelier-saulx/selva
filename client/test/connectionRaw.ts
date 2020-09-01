import test from 'ava'
import { Connection, connections, connect } from '../src/index'
import { startRegistry, startOrigin, startReplica } from '../../server'
import './assertions'
import { wait } from './assertions'
import { join } from 'path'

const dir = join(process.cwd(), 'tmp', 'connection-raw-test')

test.serial('make a connection instance', async t => {
  startRegistry({ port: 9999 })

  console.log('create client')

  const client = connect({
    port: 9999
  })

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

  console.log('start origins')
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

  const servers1 = await client.redis.smembers(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    'servers'
  )

  console.log({ servers1 })
  await wait(1e3)

  const servers2 = await client.redis.smembers(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    'servers'
  )
  console.log({ servers2 })

  await client.redis.hset(
    {
      type: 'origin'
    },
    'flappie',
    'snurf',
    'snarx'
  )

  const originKeys = await client.redis.keys(
    {
      type: 'origin'
    },
    '*'
  )

  console.log({ originKeys })

  console.log('set snurf origin', originKeys)
  await client.redis.hset(
    {
      type: 'origin',
      name: 'snurf'
    },
    'snufels',
    'snurf',
    'snarx'
  )

  console.log('getting keys from snurf')

  const yyyy = await client.redis.keys(
    {
      type: 'origin',
      name: 'snurf'
    },
    '*'
  )

  console.log('keys from snurf', yyyy)

  console.log('remove snurf server')
  await snurfServer.destroy()
  console.log('snurf server destroyed')

  console.log('start go')
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

  console.log('get data from replicas')
  const replica = await client.redis.keys(
    {
      type: 'replica'
    },
    '*'
  )

  const oneReplica = await client.getServer(
    { type: 'replica' },
    { strict: true }
  )

  // do we want 'force replica' as option
  console.log('🚷', { oneReplica })

  const nukeReplica = async (r, cnt = 0) => {
    // console.log('gimme', await client.redis.hgetall(oneReplica, 'flappie'))
    let q = []
    for (let i = 0; i < 10000; i++) {
      q.push(client.redis.hgetall(r, ~~(1000 * Math.random()).toString(16)))
    }

    await Promise.all(q)
    // console.log('done 10k keys')

    if (cnt < 1000) {
      nukeReplica(r, ++cnt)
    } else {
      console.log('done nuking (1000 x 10k)', r)
    }
  }

  console.log('go nuke onReplica')
  nukeReplica(oneReplica)

  // now getting a replica needs to get anothert one
  const secondReplica = await client.getServer(
    { type: 'replica' },
    { strict: true }
  )

  console.log('🚷', { secondReplica })

  // selva client emit reconnect event (with descriptor)
  await wait(1000e3)

  t.pass()
})
