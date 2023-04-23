import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import getPort from 'get-port'
import { wait } from './assertions'

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
          name: { type: 'string' },
          isItTrue: { type: 'boolean' },
        },
      },
      thing2: {
        fields: {
          name: { type: 'string' },
          isItTrue: { type: 'boolean' },
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
  await t.connectionsAreEmpty()
})

test.serial.only('query refs in arrays and records', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  let n = 20
  while (n--) {
    await client.set({
      type: 'thing',
      name: 'thing ' + n,
      isItTrue: Boolean(n % 2),
    })

    await client.set({
      type: 'thing2',
      name: 'thing ' + n,
      isItTrue: Boolean(n % 2),
    })
  }

  const res = await client.get({
    things: {
      id: true,
      name: true,
      type: true,
      $list: {
        // $sort: { $field: 'id', $order: 'desc' },
        $offset: 1,
        $limit: 4,
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'thing',
            },
            {
              $field: 'isItTrue',
              $operator: '!=',
              $value: true,
            },
          ],
        },
      },
    },
  })

  const res2 = await client.get({
    things: {
      id: true,
      name: true,
      type: true,
      $list: {
        // $sort: { $field: 'id', $order: 'desc' },
        $offset: 10,
        $limit: 5,
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'thing',
            },
            {
              $field: 'isItTrue',
              $operator: '!=',
              $value: true,
            },
          ],
        },
      },
    },
  })

  console.log('???', res, res2)

  await client.destroy()
})
