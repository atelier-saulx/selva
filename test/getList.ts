import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'
import { wait } from './assertions'

let srv
test.before(async t => {
  srv = await start({ port: 6062, developmentLogging: true, loglevel: 'info' })
  await wait(500)
})

test.after(async _t => {
  const client = connect({ port: 6062 })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('get - simple $list', async t => {
  const client = connect({ port: 6062 })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      custom: {
        prefix: 'cu',
        fields: {
          name: { type: 'string' },
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
      }
    }
  })

  // Should not come here once LUA: [info] searchStr for cuB with root,cuA
  await Promise.all([
    client.set({
      $id: 'cuA',
      image: {
        thumb: 'flurp.jpg'
      },
      title: { en: 'snurf' },
      children: ['cuB', 'cuC']
    })
  ])

  t.true(true)

  await client.delete('root')

  client.destroy()
})
