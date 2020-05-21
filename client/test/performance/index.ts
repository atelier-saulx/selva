import test from 'ava'
import { connect } from '../../src/index'
import {
  startRegistry,
  startOrigin,
  startSubscriptionManager,
  startReplica
} from '@saulx/selva-server'
import { wait } from '../assertions'
import { start, stop, run } from './testRunner'

let registry

test.before(async t => {
  registry = (await start()).registry
})

test.after(async _t => {
  await stop()
})

test.serial('Perf - Set a lot of things', async t => {
  console.log('perf it!')

  const time = await run(
    async client => {
      console.log('GO')
      client.set({
        $id: 'root',
        value: ~~(Math.random() * 100)
      })
      await wait(50)
    },
    {
      clients: 2,
      time: 1e3
    }
  )

  t.true(true)
})
