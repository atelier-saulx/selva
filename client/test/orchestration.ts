import test from 'ava'
import { connect } from '../src/index'
import {
  startRegistry,
  startOrigin,
  startReplica,
  startSubscriptionManager
} from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'
import fs from 'fs'
import { join } from 'path'
import rimraf from 'rimraf'

const dir = join(process.cwd(), 'tmp', 'orchestrationtest')

const removeDump = async () => {
  if (fs.existsSync(dir)) {
    rimraf(dir, err => {
      if (err) {
        console.log('cannot remove dump')
      }
    })
  }
  await wait(1e3)
}

test.before(removeDump)
test.after(removeDump)

// need to clean dumps if testing replicas
test('Create a full cluster (replica, origin, subs manager, registry)', async t => {
  let current = await getPort()

  // need a way to change this!
  const registryAdress = async () => {
    await wait(10)
    return { port: current }
  }

  const client = connect(registryAdress)

  client
    .observe({
      name: true
    })
    .subscribe(x => {})

  const startingServers = Promise.all([
    startRegistry({ port: current }),
    startOrigin({
      dir,
      registry: registryAdress,
      default: true
    }),
    startReplica({
      dir: join(dir, 'replica1'),
      registry: registryAdress,
      default: true
    }),
    startReplica({
      dir: join(dir, 'replica2'),
      registry: registryAdress,
      default: true
    }),
    startReplica({
      dir: join(dir, 'replica3'),
      registry: registryAdress,
      default: true
    }),
    startSubscriptionManager({
      registry: registryAdress
    }),
    startSubscriptionManager({
      registry: registryAdress
    }),
    startSubscriptionManager({
      registry: registryAdress
    })
  ])

  const setSchema = async () => {
    return client.updateSchema({
      languages: ['en', 'de', 'nl'],
      types: {
        custom: {
          prefix: 'cu',
          fields: {
            value: { type: 'number' },
            age: { type: 'number' },
            auth: {
              type: 'json'
            },
            title: { type: 'text' },
            description: { type: 'text' },
            image: {
              type: 'object',
              properties: {
                thumb: { type: 'string' },
                poster: { type: 'string' }
              }
            }
          }
        }
      }
    })
  }

  await setSchema()

  let subsResults = []

  client
    .observe({
      $id: 'cuflap',
      title: true
    })
    .subscribe(x => {
      subsResults.push(x)
    })

  await client.set({
    $id: 'cuflap',
    title: {
      en: 'lurkert'
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'cuflap',
      title: true
    }),
    { title: { en: 'lurkert' } }
  )

  await wait(1000)

  t.deepEqual(
    subsResults,
    [{ title: { en: 'lurkert' } }],
    'correct subs results'
  )
  subsResults = []

  // awaiting here because we want to see if they are done
  const servers = await startingServers

  // now lets change the registry url
  await servers[0].destroy()
  await wait(1000)

  let x = current
  current = await getPort()

  servers[0] = await startRegistry({ port: current })

  await client.set({
    $id: 'cuflap',
    title: {
      en: 'snurkels'
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'cuflap',
      title: true
    }),
    { title: { en: 'snurkels' } }
  )

  // speed this up!
  await wait(15000)

  // makes it easier to test things

  t.deepEqual(
    subsResults,
    [{ title: { en: 'snurkels' } }],
    'correct subs results after restarting of the registry'
  )

  servers.forEach((s, index) => {
    const [_, port] = s.selvaClient.redis.registry.id.split(':')
    t.is(Number(port), current, 'correct changed port of registry on ' + index)
  })

  await wait(1000)

  // change origin

  // now lets change the origin url
  await servers[1].destroy()
  await wait(1000)

  servers[1] = await startOrigin({
    default: true,
    registry: registryAdress,
    dir: join(dir, 'neworigin')
  })

  await wait(5000)

  await setSchema()

  const schema = await client.getSchema()

  t.true(!!schema)

  await client.set({
    $id: 'cuflap',
    title: {
      en: 'yuzi'
    }
  })

  const yuzi = await client.redis.hgetall(
    {
      type: 'replica'
    },
    'cuflap'
  )

  t.is(yuzi['title.en'], 'yuzi', 'gets yuzi from a replica after reset')

  await wait(15000)

  console.log(subsResults)

  // now thu sneeds a new title

  // now lets subscribe and balance those subscriptions AND test!

  servers.forEach(s => {
    s.destroy()
  })

  await wait(5000)

  t.pass('Did not crash')
})
