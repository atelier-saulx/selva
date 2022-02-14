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
          },
        },
      },
    })
  } catch (err) {
    console.error('????', err)
  }

  client.validator = (schema, type, path, value) => {
    console.info('yes', schema, type, path, value)
    return true
  }

  // await client.set({
  //   $id: 'root',
  // })

  console.info('yes?')
  await client.set({
    type: 'thing',
    image: 'yes',
  })

  await wait(1000)

  // add some tests for it

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()

  t.pass('yes')
})

// test.serial('schemas - updates and migrations', async (t) => {
