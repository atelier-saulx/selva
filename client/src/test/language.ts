import test from 'ava'
import { connect } from '../src/index'
import { start, startOrigin } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let srv2
let port: number
test.before(async () => {
  port = await getPort()
  srv = await start({
    port
  })
  console.log('ok server started!')
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial.skip('language in all types of objects', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      blurf: {
        prefix: 'bl',
        fields: {
          title: { type: 'text' },
          randoObject: {
            type: 'object',
            properties: {
              title: { type: 'text' }
            }
          },
          randoArray: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                title: { type: 'text' }
              }
            }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'bl1',
    $language: 'en',
    title: 'engTitle',
    randoObject: { title: 'randoObject.engTitle' },
    randoArray: [{ title: 'randoArray.engTitle' }]
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'bl1',
      $language: 'en',
      title: true,
      randoObject: true,
      randoArray: true
    }),
    {
      title: 'engTitle',
      randoArray: [
        {
          title: 'randoArray.engTitle'
        }
      ],
      randoObject: {
        title: 'randoObject.engTitle'
      }
    }
  )
})
