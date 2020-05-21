import test from 'ava'
import { connect } from '../../src/index'
import {
  startRegistry,
  startOrigin,
  startSubscriptionManager,
  startReplica
} from '@saulx/selva-server'
import './assertions'
import { start, stop, run } from './testRunner'

let registry

test.before(async t => {
  registry = (await start()).registry
})

test.after(async _t => {
  await stop()
})

test.serial('Perf - Set a lot of things', async t => {
  const time = await run(
    client => {
      client.set({
        $id: 'root',
        value: ~~(Math.random() * 100)
      })
    },
    {
      clients: 5,
      time: 10e3
    }
  )

  t.true(true)
})
