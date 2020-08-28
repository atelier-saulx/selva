import test from 'ava'
import { Connection, connections, connect } from '../src/index'
import { startRegistry, startOrigin, startReplica } from '../../server'
import './assertions'
import { wait } from './assertions'
import { join } from 'path'

const dir = join(process.cwd(), 'tmp', 'connection-raw-test')

test.serial('make a connection instance', async t => {
  startRegistry({ port: 9999 })

  console.log('go time')

  const client = connect({
    port: 9999
  })

  client.registryConnection.on('connect', () => {
    console.log('ok connect')
  })

  client.registryConnection.on('hard-disconnect', () => {
    console.log('ok destruction going on')
  })

  client.registryConnection.on('disconnect', () => {
    console.log('ok dc')
  })

  client.registryConnection.on('destroy', () => {
    console.log('destroy it')
  })

  await wait(3e3)

  console.log('make new in it server time')
  startOrigin({
    default: true,
    registry: { port: 9999 }
  })

  const snurfServer = await startOrigin({
    name: 'snurf',
    registry: { port: 9999 }
  })

  const x = await client.redis.keys(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    '*'
  )

  const xx = await client.redis.smembers(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    'servers'
  )

  console.log('x', x, xx)
  await wait(3e3)

  const xxx = await client.redis.smembers(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    'servers'
  )
  console.log(xxx)

  const xxxxx = await client.redis.hset(
    {
      type: 'origin'
    },
    'flappie',
    'snurf',
    'snarx'
  )

  const xxxx = await client.redis.keys(
    {
      type: 'origin'
    },
    '*'
  )

  console.log('get dat origin', xxxx)
  await client.redis.hset(
    {
      type: 'origin',
      name: 'snurf'
    },
    'snufels',
    'snurf',
    'snarx'
  )

  const yyyy = await client.redis.keys(
    {
      type: 'origin',
      name: 'snurf'
    },
    '*'
  )

  console.log(yyyy)

  console.log('remove this biaaatch')
  await snurfServer.destroy()
  console.log('drestucto')

  console.log('start go')
  startReplica({
    registry: { port: 9999 },
    default: true,
    dir: join(dir, 'replica1')
  })

  console.log('trying replica for you')
  const replica = await client.redis.keys(
    {
      type: 'replica'
    },
    '*'
  )

  console.log('replica result', replica)
  // selva client emit reconnect event (with descriptor)
  await wait(1000e3)

  t.pass()
})
