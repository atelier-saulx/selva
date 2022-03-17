import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

test.serial('schemas - meta', async (t) => {
  const port = await getPort()
  const server = await start({
    port,
  })
  const client = connect({ port })

  try {
    await client.updateSchema({
      // allways set eng
      languages: ['en'],
      types: {
        thing: {
          meta: {
            flapflap: true,
          },
        },
      },
    })
  } catch (err) {}

  const x = await client.getSchema()

  t.true(x.schema.types.thing.meta.flapflap)

  try {
    await client.updateSchema({
      types: {
        thing: {
          meta: {
            hello: 'xyz',
          },
        },
      },
    })
  } catch (err) {}

  const y = await client.getSchema()

  t.not(x.schema.sha, y.schema.sha)
  t.is(y.schema.types.thing.meta.hello, 'xyz')

  try {
    await client.updateSchema({
      // allways set eng
      languages: ['en'],
      types: {
        thing: {
          fields: {
            snapje: {
              type: 'string',
              meta: 'fun',
            },
          },
        },
      },
    })
  } catch (err) {}

  const z = await client.getSchema()

  t.is(z.schema.types.thing.fields.snapje.meta, 'fun')

  try {
    await client.updateSchema({
      // allways set eng
      languages: ['en'],
      types: {
        thing: {
          fields: {
            snapje: {
              type: 'string',
              meta: 'flap',
            },
          },
        },
      },
    })
  } catch (err) {}

  const zz = await client.getSchema()

  t.is(zz.schema.types.thing.fields.snapje.meta, 'flap')

  await wait(1000)

  await client.destroy()
  await server.destroy()
  await t.connectionsAreEmpty()
})
