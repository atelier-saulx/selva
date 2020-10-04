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
})

test.after(async t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('yes', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: {
        title: { type: 'text' }
      }
    }
  })

  await wait(500)
  let cnt = 0
  client.subscribeSchema().subscribe(schema => {
    cnt++
  })

  await wait(500)

  client.subscribeSchema().subscribe(schema => {
    cnt++
  })

  await wait(500)

  t.is(cnt, 2)

  await client.destroy()
})
