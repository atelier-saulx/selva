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
      dictionary: {
        prefix: 'di',
        fields: {
          words: {
            type: 'object',
            properties: {
              rando: { type: 'text' }
            }
          }
        }
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

test.serial('$language should be applied in nested text', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const dictionary = await client.set({
    $language: 'en',
    type: 'dictionary',
    words: {
      rando: 'my word'
    }
  })

  t.deepEqual(
    await client.get({
      $id: dictionary,
      $language: 'en',
      words: true
    }),
    { words: { rando: 'my word' } }
  )
})
