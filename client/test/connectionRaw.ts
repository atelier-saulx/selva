import test from 'ava'
import { Connection, connections, connect } from '../src/index'
import './assertions'
import { wait } from './assertions'

test.serial('make a connection instance', async t => {
  const client = connect({
    port: 9999
  })

  console.log('go time')

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

  // selva client emit reconnect event (with descriptor)
  await wait(1000e3)

  // const nConnection = new Connection()
  console.log('Make it nice for you nice 😁')

  t.pass()
})
