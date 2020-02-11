import test, { ExecutionContext } from 'ava'
import './assertions'
import { connect, SelvaClient } from '../src/index'
import { start } from 'selva-server'
import { wait } from './assertions'

let srv
test.before(async t => {
  srv = await start({
    port: 6092,
    loglevel: 'info',
    developmentLogging: true
  })

  await wait(200)

  const client = connect({
    port: 6092
  })

  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: { value: { type: 'number' } }
    },
    types: {
      thing: {
        fields: {
          numbers: {
            type: 'set',
            items: {
              type: 'digest'
            }
          }
        }
      }
    }
  })

  await client.destroy()
})

test.serial('basic set', async t => {
  const client = connect({
    port: 6092
  })

  const id = await client.set({
    type: 'thing',
    numbers: {
      $add: ['1', '2', '100']
    }
  })

  const { numbers } = await client.get({ $id: id, numbers: true })

  t.deepEqualIgnoreOrder(numbers, [
    'b3e4d7a1889fdb2222b848c6a3d6ab029cc18b87f40d5c086c7456c87bbd3c89',
    '814f4c8863d6621f81ab494460f6c4fa66a86208839aba687a2da37db21e99d5',
    '9211490f0570d4b8288817d66fa39bebabee80ae64567baeb00ee6afe392f200'
  ])
})
