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
        text: { type: 'text' }
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

test.serial('subscribe empty', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const q = {
    $id: 'root',
    $language: 'br',
    text: true
  }

  console.log('---->', await client.get(q), client.schemas)

  client.observe(q).subscribe(res => {
    console.log('===>', res)
  })

  await wait(500)
})
