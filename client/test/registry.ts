import test from 'ava'
import { connect } from '../src/index'
import { startRegistry, startOrigin } from '@saulx/selva-server'
import { wait } from './assertions'

// let srv

test('hello ik ben één test', async t => {
  const registry = await startRegistry({})

  const origin = await startOrigin({
    default: true,
    registry: { port: registry.port, host: registry.host }
  })

  const client = connect({ port: registry.port })

  const x = await client.redis.smembers({ type: 'registry' }, 'servers')

  console.log('---------->', x)
  // console.log(await xy.redis.hmget({ type: 'registry' }))

  t.true(true)
})

// connect

// simple redis functions e.g. hget etc

// origin
// cache
// replica
// registry

// subscribe handler
// make subs a bit cleaner

// redis  manages a quue for dc
// redis-client manages buffer and reconn queues

// redis-client allways has a subscriber and publisher client

// if connecting to cache (used for subscriptions) handle things alittle bit different in 'redis'
