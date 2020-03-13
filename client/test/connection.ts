import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

test('Connect and re-connect', async t => {
  let current = await getPort()
  const client = connect(async () => {
    return { port: current }
  })

  const server = await start({ port: current })

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

  await wait(1e3)
  console.log('destroying server')
  current = await getPort()
  await server.destroy()
  console.log('server destroyed')

  await wait(2e3)
  const server2 = await start({ port: current })

  // how to test?

  await wait(2e3)

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

  t.deepEqual(
    await client.get({
      $id: 'cuflap',
      title: true
    }),
    {}
  )

  server2.destroy()
  await wait(1e3)

  client.updateSchema({
    types: {
      flurp: {
        prefix: 'fl',
        fields: {
          snurk: { type: 'string' }
        }
      }
    }
  })

  console.log('SET SNURK')
  client.set({
    $id: 'flap',
    snurk: 'snurk it 1'
  })

  console.log('long wait')
  await wait(10e3)
  const server3 = await start({ port: current })

  await wait(3e3)
  console.log('RECONNECT')

  const item = await client.get({ $id: 'flap', snurk: true })

  t.deepEqual(item, {
    snurk: 'snurk it 1'
  })

  console.log(item)

  server3.destroy()
})
