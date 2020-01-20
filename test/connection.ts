import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import { wait } from './assertions'

test('Connect and re-connect', async t => {
  let current = { port: 6066 }

  const client = await connect(async () => {
    return current
  })

  const server = await start({ port: 6066, modules: ['redisearch', 'selva'] })

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

  await server.destroy()

  await wait(1e3)
  current = { port: 6067 }
  const server2 = await start({ port: 6067, modules: ['redisearch'] })

  t.deepEqual(
    await client.get({
      $id: 'cuflap',
      title: true
    }),
    {}
  )

  server2.destroy()
})
