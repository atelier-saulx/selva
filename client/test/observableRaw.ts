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

const dir = join(process.cwd(), 'tmp', 'observable-raw-test')

test.before(removeDump(dir))
test.after(removeDump(dir))

test.serial('Make some observables', async t => {
  let registry = await startRegistry({ port: 9999 })
  const connectOpts = { port: 9999 }
  let origin = await startOrigin({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'replicarestarterorigin')
  })

  const replica = await startReplica({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'replicarestarter')
  })

  replica.on('error', err => {
    console.log(err)
  })

  const client = connect({ port: 9999 })

  t.pass()
})
