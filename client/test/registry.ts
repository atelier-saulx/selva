import test from 'ava'
import { connect, constants } from '../src/index'
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

  const y = await Promise.all(
    x.map(v => client.redis.hgetall({ type: 'registry' }, v))
  )

  console.log('---------->', y)

  // client.redis.subscribe({ type: 'registry' }, constants.REGISTRY_UPDATE_STATS)

  client.redis.on({ type: 'registry' }, 'message', v => {
    // console.log('shiiit', v)
  })

  await wait(1000)

  console.log('UPDATE SCHEMA')
  await client.updateSchema(
    {
      types: {
        helloType: {
          prefix: 'ht',
          fields: {
            title: { type: 'text' },
            value: { type: 'number' }
          }
        }
      }
    },
    'registry'
  )

  console.log('getSchema()', await client.getSchema('registry'))

  await client.redis.hmset(
    { name: 'registry' },
    'ht1',
    'value',
    1,
    'title.en',
    'murk'
  )

  const xx = await client.get({
    $db: 'registry',
    $id: 'ht1',
    $language: 'en',
    value: true,
    title: true
  })

  console.log(xx)

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
