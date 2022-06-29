import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
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

test.serial('sort by number with missing field values', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    value: 1,
  })
  await client.set({
    type: 'match',
    value: 2,
  })
  await client.set({
    type: 'match',
  })
  await client.set({
    type: 'match',
    value: 4,
  })
  await client.set({
    type: 'match',
  })

  t.deepEqual(
    await client.get({
      children: {
        value: true,
        $list: {
          $sort: {
            $field: 'value',
            $order: 'asc',
          },
        },
      },
    }),
    { children: [ { value: 1 }, { value: 2 }, { value: 4 }, {}, {} ] }
  )

  await client.destroy()
})
