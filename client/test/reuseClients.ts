import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

test('Connect and re-connect', async t => {
  let current = await getPort()
  const client = connect(async () => {
    return { port: current }
  })

  const client2 = connect(async () => {
    return { port: current }
  })

  const server = await start({ port: current })

  // client === client2

  await wait()

  await server.destroy()

  console.log('hello do it')

  t.true(true)
  // same port means same
})
