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
    languages: ['en'],
    rootType: {
      fields: {
        title: { type: 'text' }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('multiple subscribes on same thing', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const root = await client.set({
    $id: 'root',
    $language: 'en',
    title: 'Home'
  })

  let n = 3

  t.plan(6)

  while (n--) {
    const obs = client.observe({
      $id: 'root',
      id: true,
      title: true
    })

    let i = n
    obs.subscribe(data => {
      t.pass('subscribe fires')
    })

    await wait(500)
  }

  await client.set({
    $id: 'root',
    $language: 'en',
    title: 'Home!'
  })
  await wait(500)
})
