import test from 'ava'
import { Connection, connections, connect } from '../src/index'
import './assertions'
import { wait } from './assertions'

console.log('go time')

test.serial('make a connection instance', async t => {
  const client = connect({
    port: 9999
  })

  console.log('go time')

  client.registryConnection.on('connect', () => {
    console.log('ok connect')
  })

  await wait(1e3)

  // const nConnection = new Connection()
  console.log('Make it nice for you nice 😁')

  t.pass()
})
