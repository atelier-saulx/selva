import test from 'ava'
import {
  moduleId as parentModuleId,
  connect,
  connections,
  observables
} from '@saulx/selva'
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

test.serial('Make some observables', async t => {
  const port = await getPort()

  let registry = await startRegistry({ port })
  const connectOpts = { port }
  let origin = await startOrigin({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'observablesgotime')
  })

  const client = connect({ port })

  t.pass()
})
