import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
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
    languages: ['en', 'de', 'nl'],
    types: {
      flurps: {
        prefix: 'fl',
        fields: {
          x: { type: 'string' },
          f: {
            type: 'object',
            properties: {
              a: { type: 'string' },
              b: { type: 'string' },
              c: { type: 'string' },
              d: { type: 'string' }
            }
          }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('get - correct order', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    $id: 'flA',
    f: {
      d: 'a',
      b: 'b',
      c: 'c'
    }
  })

  const x = await client.get({
    $id: 'flA',
    f: true
  })

  await client.set({
    $id: 'flA',
    f: {
      d: 'xxxx',
      a: 'x'
    }
  })

  const y = await client.get({
    $id: 'flA',
    f: true
  })

  for (let i = 0; i < 1; i++) {
    await client.set({
      $id: 'flA',
      x: i + ''
    })

    const z = await client.get({
      $id: 'flA',
      f: true
    })
    t.deepEqual(z, y)
  }

  await client.destroy()
})
