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

  client.updateSchema({
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

  // client.validator = (schema, type, path, value) => {
  //  somethign like this
  // }

  await client.set({
    $id: 'root',
  })

  await wait(1000)

  // add some tests for it
  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()
})

// test.serial('schemas - updates and migrations', async (t) => {
