import test from 'ava'
import { connect } from '../src/index'
import { SelvaServer, start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv: SelvaServer
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      aType: {
        prefix: 'at',
        fields: {
          name: { type: 'string' },
          value: { type: 'number' },
          date: { type: 'timestamp' },
        },
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.destroy()
  await srv.destroy()
})

test.serial('simple find subscription time based', async (t) => {
  t.timeout(3000)

  const client = connect({ port })

  const ts = Date.now()
  await client.set({
    type: 'aType',
    name: 'name',
    date: ts + 1000,
  })
  await wait(200)

  let resultAmount = 0
  client
    .observe({
      items: {
        id: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'aType',
              },
              {
                $field: 'date',
                $operator: '>',
                $value: 'now',
              },
            ],
          },
        },
      },
    })
    .subscribe((result) => {
      resultAmount = result.items?.length
    })

  await wait(200)
  t.is(resultAmount, 1)
  await wait(1200)
  t.is(resultAmount, 0)
})
