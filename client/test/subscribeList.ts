import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

import { performance, PerformanceObserver } from 'perf_hooks'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          date: { type: 'timestamp', search: true }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('list subscription', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )
  await wait(500)

  await client.set({
    $id: 'root',
    children: [
      {
        $id: 'mabitch',
        name: 'hello shurk',
        date: 1
      },
      {
        $id: 'mabitcha',
        name: 'hello xxxx',
        date: 2
      }
    ]
  })

  const o = await client.observe({
    $id: 'root',
    children: {
      name: true,
      date: true,
      $list: {
        $sort: { $field: 'date', $order: 'asc' }
      }
    }
  })

  const x = await client.get({
    $id: 'root',
    children: {
      name: true,
      date: true,
      $list: {
        $sort: { $field: 'date', $order: 'asc' }
      }
    }
  })
  console.log(x)

  await wait(500)

  o.subscribe(c => {
    console.log('!', c)
  })

  t.true(true)

  await wait(500)
})
