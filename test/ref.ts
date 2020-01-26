import test from 'ava'
import { connect } from '../src/index'
import { start } from 'selva-server'
import './assertions'

test.before(async t => {
  await start({
    port: 7072,
    developmentLogging: true,
    loglevel: 'info'
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect({ port: 7072 })
  await client.updateSchema({
    languages: ['en', 'en_us', 'en_uk', 'de', 'nl'],
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          name: { type: 'string' },
          strVal: { type: 'string' },
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
          description: { type: 'text' }
        }
      }
    }
  })

  await client.destroy()
})

test.serial.only('string field ref', async t => {
  const client = connect({ port: 7072 })

  await client.set({
    $id: 'viA',
    name: 'yesh',
    title: {
      en: 'nice!'
    },
    strVal: { $ref: 'name' },
    value: 25
  })

  t.deepEqual(
    await client.get({
      $id: 'viA',
      id: true,
      value: true,
      name: true,
      strVal: true
    }),
    {
      id: 'viA',
      value: 25,
      name: 'yesh',
      strVal: 'yesh'
    }
  )
})

test.serial('text field ref', async t => {
  const client = connect({ port: 7072 })

  await client.set({
    $id: 'viB',
    title: {
      en: 'nice!',
      en_uk: { $ref: 'title.en' },
      en_us: { $ref: 'title.en' }
    },
    value: 25,
    auth: {
      // role needs to be different , different roles per scope should be possible
      role: {
        id: ['root'],
        type: 'admin'
      }
    }
  })

  t.deepEqual(
    await client.get({
      $id: 'viB',
      id: true,
      value: true
    }),
    {
      id: 'viB',
      title: {
        en: 'nice!',
        en_uk: 'nice!',
        en_us: 'nice!'
      },
      value: 25
    }
  )
})
