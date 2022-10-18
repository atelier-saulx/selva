import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait, removeDump } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de'],
    types: {
      thing: {
        fields: {
          myarray: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                myfield: {
                  type: 'string',
                },
              },
            },
          },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('schema updates!', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema(
    {
      languages: ['en', 'de'],
      types: {
        thing: {
          fields: {
            myarray: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  myfield: {
                    $delete: true,
                  },
                },
              },
            },
          },
        },
      },
    },
    'default',
    true
  )

  console.log(JSON.stringify((await client.getSchema()).schema, null, 2))

  const { schema } = await client.getSchema()

  // @ts-ignore
  t.false(!!schema.types.thing.fields.myarray.items.properties?.myfield)

  await client.destroy()
})
