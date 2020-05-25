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

  await client.updateSchema({
    languages: ['en'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' }
        }
      }
    }
  })

  await client.set({
    $id: 'ma1',
    aliases: { $add: 'thingy' }
  })

  const x = await client.set({
    $alias: 'thingy',
    title: 'yesh',
    $language: 'en'
  })

  t.deepEqualIgnoreOrder(x, 'ma1')

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'thingy',
      title: true
    }),
    {
      title: 'yesh'
    }
  )
})
