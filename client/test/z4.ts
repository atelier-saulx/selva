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
    },
    types: {
      rando: {
        prefix: 'ra',
        fields: {
          title: { type: 'text' },
          niceSet: {
            type: 'set',
            items: {
              type: 'string'
            }
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

test.serial('yes', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    type: 'rando',
    $id: 'ra1',
    $language: 'en'
  })
  const results = []

  client
    .observe({
      $id: 'ra1',
      aliases: true,
      niceSet: true
    })
    .subscribe(res => {
      results.push(res)
      console.log('--------->', res)
    })

  await wait(500)

  await client.set({
    $id: 'ra1',
    aliases: ['a'],
    niceSet: ['a']
  })

  await wait(500)

  await client.set({
    $id: 'ra1',
    aliases: ['b'],
    niceSet: ['b']
  })

  await wait(500)

  t.deepEqual(results, [
    { niceSet: [], aliases: [] },
    { niceSet: ['a'], aliases: ['a'] },
    { niceSet: ['b'], aliases: ['b'] }
  ])
})
