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
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          name: { type: 'string' },
          value: { type: 'number' },
          status: { type: 'number' },
          date: { type: 'number' },
        },
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.info('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscription validation error', async (t) => {
  const client = connect({ port })
  let errorCnt = 0
  client
    .observe({
      $db: {},
    })
    .subscribe(
      () => {},
      () => {
        // console.log('yesh')
        errorCnt++
      }
    )
  client.observe({
    $db: {},
  })
  client.observe({
    $db: {},
  })
  await wait(2e3)
  t.is(errorCnt, 1)
  client.observe({
    $db: {},
  })
  client
    .observe({
      $db: {},
    })
    .subscribe(
      () => {},
      () => {
        errorCnt++
      }
    )
  await wait(2e3)
  t.is(errorCnt, 2)
  await client.destroy()
})

test.serial(
  'subscription initialization with multiple subscribers',
  async (t) => {
    const client = connect({ port })
    let cnt = 0
    const id = await client.set({
      type: 'match',
      title: { en: 'snurfels' },
    })
    client
      .observe({
        $id: id,
        title: true,
      })
      .subscribe(
        (v) => {
          cnt++
        },
        () => {
          // errorCnt++
        }
      )
    await wait(1000)
    client
      .observe({
        $id: id,
        title: true,
      })
      .subscribe(
        (v) => {
          cnt++
        },
        () => {
          // errorCnt++
        }
      )
    await wait(1000)
    t.is(cnt, 2)
    await client.set({
      $id: id,
      title: { en: 'snurfels22' },
    })
    await wait(1000)
    t.is(cnt, 4)
    await client.destroy()
  }
)

test.serial('subscription error on subs manager', async (t) => {
  const client = connect({ port })
  const results = []
  client
    .observe({
      $language: 'en',
      $id: 'mayuzi',
      yizi: {
        title: true,
        $inherit: {
          $item: 'club',
        },
      },
      title: true,
    })
    .subscribe(
      (v) => {
        results.push(v)
      },
      (err) => {
        console.error(err)
        // errorCnt++
      }
    )
  await wait(1000)
  t.deepEqual(results, [{ $isNull: true }], 'correct isNull on unexisting item')

  await client.destroy()
})
