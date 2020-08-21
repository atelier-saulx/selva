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
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
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

test.serial('subscription validation error', async t => {
  const client = connect({ port })

  // make this nice
  client
    .observe({
      $db: {}
    })
    .subscribe(
      v => {
        console.log('its fine', v)
      },
      err => console.log('Error', err)
    )

  client.observe({
    $db: {}
  })

  client.observe({
    $db: {}
  })
  // make validation error

  await wait(2e3)

  client.observe({
    $db: {}
  })

  await wait(2e3)

  t.true(true)
})
