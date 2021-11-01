import test from 'ava'
import { connect } from '@saulx/selva'
import { startRegistry, startOrigin } from '../../server'
import { wait } from './assertions'
import getPort from 'get-port'

test.serial('connection min', async (t) => {
  await wait(2e3)
  const port = await getPort()

  const registry = await startRegistry({ port })

  console.info('go go go client')
  const client = connect({
    port,
  })

  console.info('YES CONNECT IT!')

  await wait(10000)

  console.log('wait is done')

  const origin = await startOrigin({
    default: true,
    registry: { port },
    dir: null,
  })

  await registry.destroy()
  await origin.destroy()
  await client.destroy()

  await t.connectionsAreEmpty()
})
