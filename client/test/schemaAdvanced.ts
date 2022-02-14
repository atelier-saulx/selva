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
  // schema updates (and migrations)

  // custom vaildation

  try {
    await client.updateSchema({
      types: {
        thing: {
          prefix: 'th',
          fields: {
            image: {
              type: 'string',
              meta: 'image',
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

  client.validator = (schema, type, path, value) => {
    console.info('yesVALIDATOPR', schema, type, path, value)
    // custom messages as well...
    return true
  }

  // await client.set({
  //   $id: 'root',
  // })

  await client.set({
    type: 'thing',
    image: 'yes',
    obj: {
      flap: 'x',
      x: {
        snurk: 'hello',
      },
    },
  })

  await wait(1000)

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass('yes')
})

// test.serial('schemas - updates and migrations', async (t) => {
