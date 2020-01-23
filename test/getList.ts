import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'

test.before(async t => {
  await start({ port: 6062, modules: ['redisearch', 'selva'] })
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
