import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  const client = connect({ port })
  await client.updateSchema({
    rootType: {
      fields: {
        menu: { type: 'references' }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('correct validation #1', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  try {
    const res = await client.get({
      pollId: 'RandomPollName'
    })
  } catch (e) {
    console.error(e)
    t.assert(
      e.stack,
      `Field .pollId should be a boolean or an object, got RandomPollName`
    )
  }
})

test.serial('correct validation #2', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  try {
    const res = await client.set({ $alias: 'RandomPollName', children: [] })
    t.pass()
  } catch (e) {
    t.fail()
  }
})
