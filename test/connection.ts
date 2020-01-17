import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import { wait } from './assertions'

test('Connect and re-connect', async t => {
  const client = await connect(async () => {
    console.log('connect it')
    return { port: 6066 }
  })

  const server = await start({ port: 6066, modules: ['redisearch'] })

  client.set({
    $id: 'cuflap',
    title: {
      en: 'lurkert'
    }
  })

  // add these!!!
  // client.isConnected

  // client.on('connect', () => {})

  await wait(1e3)
  const result = await client.get({
    $id: 'cuflap',
    title: true
  })

  // FIXME: real assertion
  t.is(true, true)

  console.info('???', result)
})
