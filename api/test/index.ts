import test from 'ava'
import './assertions'
import { connect } from '@saulx/selva'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'
import { start as apiStart } from '../src/index'
import fetch from 'node-fetch'

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' }
          }
        }
      }
    },
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } },
              dung: { type: 'number' },
              dang: {
                type: 'object',
                properties: {
                  dung: { type: 'number' },
                  dunk: { type: 'string' }
                }
              },
              dunk: {
                type: 'object',
                properties: {
                  ding: { type: 'number' },
                  dong: { type: 'number' }
                }
              }
            }
          },
          dong: { type: 'json' },
          dingdongs: { type: 'array', items: { type: 'string' } },
          refs: { type: 'references' },
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      club: {
        prefix: 'cl',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' }
        }
      },
      yesno: {
        prefix: 'yn',
        fields: {
          bolYes: { type: 'boolean' },
          bolNo: { type: 'boolean' }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('test api ping/pong', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, srvPort)

  const res = await fetch(`http://localhost:${srvPort}/ping`, {
    method: 'POST',
    body: 'hellooo'
  })

  const body = await res.text()
  console.log('DERP', body)
  t.is(body, 'hellooo')

  cleanup()
})

test.serial('get $value through api', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, srvPort)

  const res = await fetch(`http://localhost:${srvPort}/get`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      yesh: { $value: 'hello' }
    })
  })

  const body = await res.json()
  t.deepEqualIgnoreOrder(body, {
    $isNull: true,
    yesh: 'hello'
  })

  cleanup()
})

test.serial('set and get simple through api', async t => {
  const srvPort = await getPort()
  const cleanup = apiStart({ port }, srvPort)

  await fetch(`http://localhost:${srvPort}/set`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'maMatch1',
      title: {
        en: 'yes en',
        de: 'ja de'
      }
    })
  })

  const res = await fetch(`http://localhost:${srvPort}/get`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      $id: 'maMatch1',
      $language: 'de',
      id: true,
      title: true
    })
  })

  const body = await res.json()
  t.deepEqualIgnoreOrder(body, {
    id: 'maMatch1',
    title: 'ja de'
  })

  cleanup()
})
