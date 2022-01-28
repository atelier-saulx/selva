import test from 'ava'
import { connect } from '../src/index'
import { startRegistry, startSubscriptionManager } from '@saulx/selva-server'
import { worker } from './assertions'
import getPort from 'get-port'
import { wait } from '../src/util'

let srv
let s
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await startRegistry({
    port,
  })

  s = await startSubscriptionManager({ registry: { port } })
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await s.destroy()
  await t.connectionsAreEmpty()
})

test.serial('recon to new origin', async (t) => {
  const client = connect({ port })
  const [, , kill] = await worker(
    async ({ connect, startOrigin }, { port }) => {
      const client = connect({ port })
      const s = await startOrigin({
        name: 'default',
        registry: {
          port,
        },
      })
      await client.updateSchema({
        languages: ['en'],
        types: {
          thing: {
            prefix: 'th',
            fields: {
              name: { type: 'string' },
            },
          },
        },
      })
      await client.set({ type: 'thing', name: 'yes' })

      return async () => {
        await s.destroy()
      }
    },
    { port }
  )
  const x = await client.get({
    $id: 'root',
    children: true,
  })
  console.info('GET RESULT', x)

  let p2 = await getPort({ port: client.servers.origins.default.port + 100 })

  kill()

  await wait(5000)

  const [, , kill2] = await worker(
    async ({ connect, startOrigin }, { port, p2 }) => {
      const client = connect({ port })
      await startOrigin({
        port: p2,
        name: 'default',
        registry: {
          port,
        },
      })
      await client.updateSchema({
        languages: ['en'],
        types: {
          thing: {
            prefix: 'th',
            fields: {
              name: { type: 'string' },
            },
          },
        },
      })
      await client.set({ type: 'thing', name: 'no!' })
    },
    { port, p2 }
  )

  const x2 = await client.get({
    $id: 'root',
    children: true,
  })

  console.info()
  console.info('GET RESULT FLAP2', x2)

  await client.destroy()

  t.pass()
})
