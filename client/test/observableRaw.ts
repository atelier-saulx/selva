import test from 'ava'
import { connect, connections, Observable, observables } from '@saulx/selva'
import {
  startRegistry,
  startOrigin,
  startReplica,
  startSubscriptionManager
} from '../../server/dist'
import './assertions'
import { wait, worker, removeDump } from './assertions'
import { join } from 'path'
import getPort from 'get-port'

const dir = join(process.cwd(), 'tmp', 'observable-raw-test')

test.before(removeDump(dir))
test.after(removeDump(dir))

// const x= '/dahsbo/:id/:smurk'.replace(/:s*.*?(?=\s*\/)/g, 'lur')

// in: '/dahsbo/:id/:smurk' out: '/dahsbo/lur/lur'

test.serial('Make some observables', async t => {
  // maybe run all the servers in workers
  const port = await getPort()
  const registry = await startRegistry({ port })
  const connectOpts = { port }
  const origin = await startOrigin({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'observablesgotime')
  })

  const client = connect({ port })

  // what to do here?
  // client.observe()

  t.pass()
})
