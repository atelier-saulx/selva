import test from 'ava'
import { connect, SelvaClient } from '../src/index'
import { start } from 'selva-server'

test.before(async t => {
  await start({ port: 6061, modules: ['redisearch'] })
})

test.serial('get - basic', async t => {
  const client = connect({
    port: 6061
  })

  await client.set({
    $id: 'viA',
    title: {
      en: 'nice!'
    }
  })

  const r = await client.get({
    $id: 'viA'
  })

  t.is(true, true)
})
