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
    languages: ['en', 'de'],
    types: {
      match: {
        fields: {
          title: {
            type: 'text',
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          }
        }
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

test.serial('handle empty objects for text', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const id = await client.set({
    type: 'match',
    title: {}
  })

  try {
    await client.get({ $id: id, title: true })
    t.pass()
  } catch (e) {
    console.error(e)
    t.fail()
  }
})
