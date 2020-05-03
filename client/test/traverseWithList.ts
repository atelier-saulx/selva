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
  srv = await start({ port })

  const client = connect({ port })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number', search: true },
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

test.serial('get - simple $list with id $traverse', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const children = []

  for (let i = 0; i < 100; i++) {
    children.push({
      $id: 'cu' + i,
      type: 'custom',
      value: i,
      name: 'flurp' + i
    })
  }

  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg'
      },
      title: { en: 'snurf' },
      children
    })
  ])

  const c = await client.get({
    $id: 'cuA',
    items: {
      name: true,
      value: true,
      $list: {
        $sort: { $field: 'value', $order: 'asc' },
        $find: {
          $traverse: ['cu1', 'cu2', 'cu3']
        }
      }
    }
  })

  t.deepEqual(c, {
    items: [
      { value: 1, name: 'flurp1' },
      { value: 2, name: 'flurp2' },
      { value: 3, name: 'flurp3' }
    ]
  })
})
