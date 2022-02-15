import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

test.serial('schemas - custom validation', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

  try {
    await client.updateSchema({
      languages: ['en'],
      types: {
        thing: {
          prefix: 'th',
          fields: {
            image: {
              type: 'string',
              meta: 'image',
            },
            set: {
              type: 'set',
              items: {
                type: 'string',
              },
            },
            list: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  x: { type: 'number' },
                  y: { type: 'number' },
                },
              },
            },
            t: {
              type: 'text',
            },
            snurky: {
              type: 'references',
            },
            flara: {
              type: 'reference',
              bidirectional: {
                fromField: 'flarb',
              },
            },
            flarb: {
              type: 'reference',
              bidirectional: {
                fromField: 'flara',
              },
            },
            obj: {
              type: 'object',
              properties: {
                flap: {
                  type: 'string',
                },
                x: {
                  type: 'object',
                  properties: {
                    snurk: {
                      type: 'string',
                    },
                  },
                },
              },
            },
          },
        },
      },
    })
  } catch (err) {}

  const info = []

  client.validator = (schema, type, path, value, lang) => {
    info.push([type, ...path])
    return true
  }

  const id = await client.set({ type: 'thing' })

  // high level validator
  await client.set({
    type: 'thing',
    image: 'yes',
    list: [
      {
        x: 1,
        y: 1,
      },
    ],
    flara: id,
    obj: {
      flap: 'x',
      x: {
        snurk: 'hello',
      },
    },
    t: {
      en: 'flap',
    },
    set: ['yes'],
    children: {
      $add: [id],
    },
  })

  console.info(info)

  await wait(1000)

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass('yes')
})

// test.serial('schemas - updates and migrations', async (t) => {
