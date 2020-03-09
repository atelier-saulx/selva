import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'

import getPort from 'get-port'
import { wait } from './assertions'

test('can strip all non-schema fields from body', async t => {
  const port = await getPort()
  const server = await start({ port })

  const client = connect({
    port
  })

  await client.updateSchema({
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text'
          },
          value: { type: 'number' },
          ary: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                keyA: { type: 'string' }
              }
            }
          },
          obj: {
            type: 'object',
            properties: {
              keyA: { type: 'string' },
              keyB: { type: 'string' },
              keyC: {
                type: 'object',
                properties: {
                  keyA: { type: 'string' }
                }
              }
            }
          }
        }
      }
    }
  })

  const payload1 = await client.conformToSchema({
    $id: 'maTest',
    value: 1234,
    somethingOdd: 'yes this should be gone'
  })

  t.deepEqual(payload1, { $id: 'maTest', type: 'match', value: 1234 })

  const payload2 = await client.conformToSchema({
    $id: 'maTest',
    value: 1234,
    somethingOdd: 'yes this should be gone',
    obj: {
      keyA: 'yes',
      keyX: 'no',
      keyY: 'no',
      keyC: {
        keyA: 'yes',
        keyZ: 'no'
      }
    }
  })

  t.deepEqual(payload2, {
    $id: 'maTest',
    type: 'match',
    value: 1234,
    obj: {
      keyA: 'yes',
      keyC: { keyA: 'yes' }
    }
  })

  const payload3 = await client.conformToSchema({
    type: 'match',
    ary: [
      {
        keyA: 'yes',
        keyB: 'no'
      },
      { keyX: 'no' },
      { keyX: 'no', keyA: 'yes' }
    ]
  })

  t.deepEqualIgnoreOrder(payload3, {
    type: 'match',
    ary: [{ keyA: 'yes' }, {}, { keyA: 'yes' }]
  })

  await wait(100)
  await server.destroy()
})
