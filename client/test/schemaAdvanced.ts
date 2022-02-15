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
            t: {
              type: 'text',
            },
            snurky: {
              type: 'references',
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
  } catch (err) {
    console.error('????', err)
  }

  // .validate
  // .isValid({ $id: 'flap', nurp: 100 })
  //
  client.validator = (schema, type, path, value, lang) => {
    console.info('yesVALIDATOPR', type, path, value, lang)
    // custom messages as well...
    return true
  }

  const id = await client.set({ type: 'thing' })

  // high level validator
  await client.set({
    type: 'thing',
    image: 'yes',
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

  // validate children

  await wait(1000)

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass('yes')
})

// test.serial('schemas - updates and migrations', async (t) => {
