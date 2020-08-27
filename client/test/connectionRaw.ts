import test from 'ava'
import { Connection, connections, connect } from '../src/index'
import { startRegistry, startOrigin } from '../../server'
import './assertions'
import { wait } from './assertions'

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

  // const xy = await client.redis.hset(
  //   {
  //     port: 9999,
  //     host: '0.0.0.0'
  //   },
  //   'flurpypants',
  //   'x',
  //   1
  // )

  // console.log('yesh it is good', xy)
  await wait(3e3)

  console.log('make new in it server time')
  startOrigin({
    default: true,
    registry: { port: 9999 }
  })

  const x = await client.redis.keys(
    {
      port: 9999,
      host: '0.0.0.0'
    },
    '*'
  )

  console.log('x', x)

  // selva client emit reconnect event (with descriptor)
  await wait(1000e3)

  // const nConnection = new Connection()
  console.log('Make it nice for you nice 😁')

  t.pass()
})
