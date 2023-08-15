import test from 'ava'
import { connect } from '../src/index'
import { startRegistry, startSubscriptionManager } from '@saulx/selva-server'
import { worker } from './assertions'
import getPort from 'get-port'
import { wait } from '../src/util'
import { fork } from 'child_process'
import { join } from 'path'
let srv
let s
let port: number

test.before(async (t) => {
  port = 2222 // hardcoded for child_process
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

  const p2 = await getPort({ port: client.servers.origins.default.port + 100 })

  kill()

  await wait(3000)

  await worker(
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

  console.info('GET RESULT FLAP2', x2)

  await client.destroy()

  t.pass()
})

test.serial.only('recon to existing origin', async (t) => {
  const startOrigin = async () => {
    console.info('- start origin in childprocess')
    const childProcess = fork(
      join(__dirname, 'assertions/selvaOriginChildProcess.js')
    )
    await wait(1e3)
    return childProcess
  }

  let p = await startOrigin()
  const client = connect({ port })
  console.info('====================== do a get()')
  const x = await client.get({
    $id: 'root',
    id: true,
  })

  console.info('++++++++++++++++++++++ result 1', x)
  console.info('---------------------- now we kill the child_process')

  p.kill()

  await wait(1e3)
  console.info('====================== now do another get()')
  const getPromise = client
    .get({
      $id: 'root',
      id: true,
    })
    .then(async (x2) => {
      // THIS NEVER HAPPENS
      console.info('++++++++++++++++++++++ this should happen but does not', x2)
    })

  await wait(1e3)
  console.info('---------------------- now we start a new child_process')
  p = await startOrigin()

  const x3 = await client.get({
    $id: 'root',
    id: true,
  })

  console.log('///////////// this one works though', x3)

  await getPromise
  await client.destroy()
  p.kill()
  t.pass()
})
