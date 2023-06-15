import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port'

let srv
let port

test.before(async (t) => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port: port })
  await client.updateSchema({
    types: {
      thing: {
        fields: {
          name: { type: 'string' },
        },
      },
      hello: {
        fields: {
          name: { type: 'string' },
          members: {
            type: 'record',
            values: {
              type: 'object',
              properties: {
                x: { type: 'string' },
                refs: { type: 'references' },
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
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('remove object from record', async (t) => {
  const client = connect({ port: port })

  const thingId = await client.set({
    type: 'thing',
    name: 'blurp',
  })

  const id = await client.set({
    type: 'hello',
    name: 'derp',
    members: {
      0: {
        x: 'hallo',
        refs: [thingId],
      },
      1: {
        x: 'doei',
      },
    },
  })

  const res1 = await client.get({
    $id: id,
    name: true,
    members: {
      '*': {
        x: true,
        refs: true,
      },
    },
  })

  t.deepEqualIgnoreOrder(res1.members[0], { x: 'hallo', refs: [thingId] })
  t.deepEqualIgnoreOrder(res1.members[1], { x: 'doei' })

  await client.set({
    $id: id,
    members: {
      0: { $delete: true },
      1: { $delete: true },
    },
  })

  await wait(500)

  const res2 = await client.get({
    $id: id,
    name: true,
    members: {
      '*': {
        x: true,
        refs: true,
      },
    },
  })

  t.is(res2.members[1], undefined)
  t.is(res2.members[0], undefined)

  await client.destroy()
})
