import fs from 'fs'
import anyTest, { TestInterface } from 'ava'
import { connect } from '../src/index'
import { SelvaServer, startWithTimeseries } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

const test = anyTest as TestInterface<{
  srv: SelvaServer
  port: number
}>

console.log()
test.before(async (t) => {
  t.context.port = await getPort()
  t.context.srv = await startWithTimeseries({
    port: t.context.port,
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port: t.context.port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      things: {
        prefix: 'th',
        fields: {
          name: {
            type: 'string',
            timeseries: true,
          },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: t.context.port })
  await client.delete('root')
  await client.destroy()
  await t.context.srv.destroy()
  await t.connectionsAreEmpty()
})

test[
  fs.existsSync('/usr/lib/postgresql/12/bin/postgres') ||
  fs.existsSync('/usr/local/bin/docker')
    ? 'serial'
    : 'skip'
]('delete node should delete timeseries data', async (t) => {
  t.timeout(5000)
  const client = connect({ port: t.context.port })

  await client.pg.connect()

  await client.set({
    $id: 'thA',
    name: 'first',
  })
  await client.set({
    $id: 'thA',
    name: 'second',
  })
  await client.set({
    $id: 'thA',
    name: 'third',
  })

  await wait(1000)

  await client.delete({ $id: 'thA' })

  await wait(1000)

  await client.set({
    $id: 'thA',
    name: 'newFirst',
  })
  await client.set({
    $id: 'thA',
    name: 'newSecond',
  })

  const result = await client.get({
    $id: 'thA',
    name: true,
    names: {
      $field: 'name',
      $list: { $limit: 5 },
    },
  })
  t.log(JSON.stringify({ result }, null, 2))

  t.is(result.names.length, 2)

  t.teardown(async () => {
    await wait(500)
    await client.delete('root')
    await client.destroy()
  })
})
