import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let srv2
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({ port })
  srv2 = await startOrigin({ name: 'users', registry: { port } })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de'],
    types: {
      show: {
        prefix: 'sh',
        fields: {
          title: {
            type: 'text'
          }
        }
      }
    }
  })

  await client.updateSchema(
    {
      languages: ['en', 'de', 'nl'],
      types: {
        user: {
          prefix: 'us',
          fields: {
            title: { type: 'text' }
          }
        }
      }
    },
    'users'
  )

  await wait(500)

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await srv2.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscribe - should fire after creation', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const id = 'sh1'

  client
    .observe({
      $id: id,
      id: true,
      $language: 'de'
    })
    .subscribe(r => {
      console.log('other fires!', r)
    })

  await wait(5e2)

  await client.set({
    $id: id,
    $language: 'de',
    title: 'IF THIS IS UNCOMMENTED IT WORKS'
  })

  let n = 5
  t.plan(n + 1)

  client
    .observe({
      $id: id,
      title: true,
      $language: 'de'
    })
    .subscribe(r => {
      console.log('fires!', r)
      t.pass()
    })

  await wait(5e2)

  while (n--) {
    console.log('setting it!', id)
    await client.set({
      $id: id,
      $language: 'de',
      title: 'test ' + n
    })
    await wait(2e3)
  }

  await client.destroy()
})
