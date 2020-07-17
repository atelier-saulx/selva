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

test('Create a full cluster (replica, origin, subs manager, registry)', async t => {
  let current = await getPort()

  const registryAdress = async () => {
    await wait(100)
    return { port: current }
  }

  const client = connect(registryAdress)

  client
    .observe({
      name: true
    })
    .subscribe(x => {})

  const registry = await startRegistry({ port: current })

  const origin = await startOrigin({ registry: registryAdress, default: true })

  const replica = await startReplica({
    registry: registryAdress,
    default: true
  })

  // const subsManager = await startSubscriptionManager({
  //   registry: registryAdress
  // })

  // const subsManager2 = await startSubscriptionManager({
  //   registry: registryAdress
  // })

  // const subsManager3 = await startSubscriptionManager({
  //   registry: registryAdress
  // })

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
})
