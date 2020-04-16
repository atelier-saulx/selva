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

  t.true(client.redis.redis === client2.redis.redis)

  // ad some subs tests here

  await wait()

  await server.destroy()
})
