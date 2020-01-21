import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import { wait } from './assertions'

test('schemas - basic', async t => {
  let current = { port: 6066 }

  const server = await start({ port: 6066, modules: ['redisearch'] })
  const client = await connect({ port: 6066 })

  await client.set({
    $id: 'cuflap',
    title: {
      en: 'lurkert'
    }
  })

  server.destroy()
})
