import test from 'ava'
import './assertions'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  const client = connect({
    port,
  })

  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: { value: { type: 'number' } },
    },
    types: {
      thing: {
        fields: {
          numbers: {
            type: 'set',
            items: {
              type: 'digest',
            },
          },
          floats: {
            type: 'set',
            items: {
              type: 'float',
            }
          },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('basic set', async (t) => {
  const client = connect({
    port,
  })

  const id = await client.set({
    type: 'thing',
    numbers: {
      $add: ['1', '2', '100'],
    },
  })

  const { numbers } = await client.get({ $id: id, numbers: true })

  t.deepEqualIgnoreOrder(numbers, [
    'b3e4d7a1889fdb2222b848c6a3d6ab029cc18b87f40d5c086c7456c87bbd3c89',
    '814f4c8863d6621f81ab494460f6c4fa66a86208839aba687a2da37db21e99d5',
    '9211490f0570d4b8288817d66fa39bebabee80ae64567baeb00ee6afe392f200',
  ])

  await client.destroy()
})

test.serial('float sets', async (t) => {
  const client = connect({
    port,
  })

  const id1 = await client.set({
    type: 'thing',
    floats: [9001],
  })
  const { floats: floats1 } = await client.get({ $id: id1, floats: true })
  t.deepEqualIgnoreOrder(floats1, [9001])

  const id2 = await client.set({
    type: 'thing',
    floats: {
      $add: [1.5, 2, 3.5, 1.1],
    },
  })

  const { floats: floats2 } = await client.get({ $id: id2, floats: true })

  t.deepEqualIgnoreOrder(floats2, [1.5, 2, 3.5, 1.1])

  await client.set({
    $id: id2,
    floats: { $add: [NaN] }
  })
})
