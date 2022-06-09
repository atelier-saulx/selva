import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      thing: {
        prefix: 'th',
        fields: {
          formFields: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                title: { type: 'text' },
              },
            },
          },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('can use $delete inside array', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const id = await client.set({
    $language: 'en',
    type: 'thing',
    formFields: [
      {
        title: 'foo',
      },
    ],
  })

  await client.set({
    $language: 'en',
    $id: id,
    formFields: {
      $assign: {
        $idx: 0,
        $value: {
          title: { $delete: true },
        },
      },
    },
  })

  const result = await client.get({
    $id: id,
    $all: true,
  })

  t.deepEqual(result, {
    id: id,
    type: 'thing',
    formFields: [{}],
  })

  await client.destroy()
})
