import test from 'ava'
import { connect, connections, Observable } from '@saulx/selva'
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

  // do this later startReplica
  const subsmanager = await startSubscriptionManager({
    registry: connectOpts,
    dir: join(dir, 'observablesgotime')
  })

  const client = connect({ port })

  await client.updateSchema({
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    value: 1
  })

  const x = await client.get({ $id: 'root', value: true })

  t.deepEqual(x, { value: 1 })

  // what to do here?
  // client.observe()

  const obs = client.observe({
    $id: 'root',
    value: true
  })

  obs.subscribe((value, checksum, diff) => {
    console.log('yesh', value, checksum, diff)
  })

  await wait(10e3)


  t.pass()
})
