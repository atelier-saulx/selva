import test from 'ava'
import { connect } from '../src/index'
import { startRegistry } from '@saulx/selva-server'
import { worker } from './assertions'
import getPort from 'get-port'
import { wait } from '../src/util'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await startRegistry({
    port,
  })
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('recon to new origin', async (t) => {
  const client = connect({ port })

  const [, , kill] = await worker(
    async ({ connect, startOrigin }, { port }) => {
      const client = connect({ port })

      await startOrigin({
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
    },
    { port }
  )

  await wait(5e3)

  console.info(client.servers)

  const x = await client.get({
    children: true,
  })

  console.info(x)

  // kill
  //

  await client.destroy()

  t.pass()
})
