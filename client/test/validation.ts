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
  const client = connect({ port })
  await client.updateSchema({
    rootType: {
      fields: {
        menu: { type: 'references' },
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('correct validation #1', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  try {
    const res = await client.get({
      pollId: 'RandomPollName',
    })
  } catch (e) {
    t.assert(
      e.stack,
      `Field .pollId should be a boolean or an object, got RandomPollName`
    )
  }
  await client.destroy()
})

test.serial('correct validation #2', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  try {
    const res = await client.set({ $alias: 'RandomPollName', children: [] })
  } catch (e) {
    t.assert(
      e.stack,
      `.set() without the type property requires an existing record or $id to be set with the wanted type prefix. No existing id found for alias "RandomPollName"`
    )
  }
  await client.destroy()
})
