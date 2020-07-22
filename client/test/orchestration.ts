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

const dir = join(process.cwd(), 'tmp', 'orchestration')

const removeDump = () => {
  if (fs.existsSync(dir)) {
    rimraf(dir, () => {
      console.log('removed!')
    })
  }
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
      dir,
      registry: registryAdress,
      default: true
    }),
    startReplica({
      dir,

      registry: registryAdress,
      default: true
    }),
    startReplica({
      dir,
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

  await client.updateSchema({
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

  // balance replica based on cpu

  // subs manager start / restart

  servers.forEach(s => {
    s.destroy()
  })

  await wait(5000)
})
