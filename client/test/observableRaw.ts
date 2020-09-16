import test from 'ava'
import { moduleId as parentModuleId, connect, connections } from '@saulx/selva'
import { startRegistry, startOrigin, startReplica } from '../../server/dist'
import './assertions'
import { wait, worker, removeDump } from './assertions'
import { join } from 'path'
import fs from 'fs'

const dir = join(process.cwd(), 'tmp', 'connection-raw-test')

test.before(removeDump(dir))
test.after(removeDump(dir))

test.serial('Change origin and re-conn replica', async t => {
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

  await client.redis.set({ type: 'origin' }, 'f', 'snurf')

  const x = await client.redis.get({ type: 'replica', strict: true }, 'f')

  t.is(x, 'snurf')

  await wait(100)

  await origin.destroy()

  await wait(5e3)

  origin = await startOrigin({
    registry: connectOpts,
    default: true,
    dir: join(dir, 'replicarestarterorigin')
  })

  const y2 = await client.redis.get({ type: 'origin' }, 'f')

  t.is(y2, 'snurf', 'get snurf from backup on origin')

  // wait till done
  const y = await client.redis.get({ type: 'replica', strict: true }, 'f')

  // may need to remove dump
  t.is(y, 'snurf', 'get snurf from re-connected replica')

  await wait(6000)
  await replica.destroy()
  await registry.destroy()
  await origin.destroy()
  await client.destroy()
  await wait(20000)
  t.is(connections.size, 0, 'all connections removed')
})
