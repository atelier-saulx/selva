import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

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
          title: { type: 'text' },
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
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

test.serial('subscription validation error', async t => {
  const client = connect({ port })
  var errorCnt = 0
  client
    .observe({
      $db: {}
    })
    .subscribe(
      () => {},
      () => {
        console.log('yesh')
        errorCnt++
      }
    )
  client.observe({
    $db: {}
  })
  client.observe({
    $db: {}
  })
  await wait(2e3)
  t.is(errorCnt, 1)
  client.observe({
    $db: {}
  })
  client
    .observe({
      $db: {}
    })
    .subscribe(
      () => {},
      () => {
        errorCnt++
      }
    )
  await wait(2e3)
  t.is(errorCnt, 2)
})

test.only('subscription initial handling', async t => {
  const client = connect({ port })
  var errorCnt = 0

  const id = await client.set({
    type: 'match',
    title: { en: 'snurfels' }
  })

  console.log(id)

  client
    .observe({
      $id: id,
      title: true
    })
    .subscribe(
      v => {
        console.log('ok', v)
      },
      () => {
        errorCnt++
      }
    )

  await wait(1000)

  client
    .observe({
      $id: id,
      title: true
    })
    .subscribe(
      v => {
        console.log('ok2', v)
      },
      () => {
        errorCnt++
      }
    )

  await wait(1000)

  t.true(true)
})
