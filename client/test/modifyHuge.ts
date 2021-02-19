import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          ...[...Array(200)].map((_, i) => ([`value${i}`, i])).reduce((acc, [a, b]) => (acc[a] = { type: 'number' }, acc), {}),
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.after(async (_t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('set all', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    ...[...Array(200)].map((_, i) => ([`value${i}`, i])).reduce((acc, [a, b]) => (acc[a] = b, acc), {})
  })
})
