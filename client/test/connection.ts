import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

test('Connect and re-connect', async t => {
  let current = await getPort()
  const client = connect(async () => {
    console.log('connect CLIENT FN')
    await wait(100)
    return { port: current }
  })

  client
    .observe({
      name: true
    })
    .subscribe(x => {
      console.log(x)
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

  console.log('new server started should reconnect!')

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
    { $isNull: true }
  )

  server2.destroy()
  await wait(2e3)

  console.log('-----------------------------------------------')
  console.log('Update schema')
  client
    .updateSchema({
      types: {
        flurp: {
          prefix: 'fl',
          fields: {
            snurk: { type: 'string' }
          }
        }
      }
    })
    .then(v => {
      console.log('Set snurk')
      client.set({
        $id: 'flap',
        snurk: 'snurk it 1'
      })
    })

  await wait(1e3)
  console.log('Reconnect')
  const server3 = await start({ port: current })

  await wait(3e3)

  // const item = await client.get({ $id: 'flap', snurk: true })

  // t.deepEqual(item, {
  //   snurk: 'snurk it 1'
  // })

  // console.log(item)

  // server3.destroy()
})

// creates one redis instance for same port / host
