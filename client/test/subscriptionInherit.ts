import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async () => {
  port = await getPort()
  srv = await start({
    port
  })
  console.log('ok server started!')
})

test.after(async () => {
  await srv.destroy()
})

test.serial('basic inherit subscription', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        yesh: { type: 'string' },
        no: { type: 'string' },
        flapper: {
          type: 'object',
          properties: {
            snurk: { type: 'string' },
            bob: { type: 'string' }
          }
        }
      }
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' },
          flapper: {
            type: 'object',
            properties: {
              snurk: { type: 'string' },
              bob: { type: 'string' }
            }
          }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    yesh: 'yesh',
    no: 'no'
  })

  await client.set({
    $id: 'yeA',
    yesh: 'yesh a'
  })

  await client.set({
    $id: 'yeB',
    parents: ['yeA']
  })

  const observable = await client.observe({
    $id: 'yeB',
    yesh: { $inherit: true }
  })

  const results = []

  const subs = observable.subscribe(p => {
    console.log(p)
    results.push(p)
  })

  await wait(1000)

  await client.set({
    $id: 'yeA',
    yesh: 'yesh a!'
  })

  await wait(1000)

  await client.set({
    $id: 'yeB',
    yesh: 'yesh b'
  })

  await wait(1000)

  t.deepEqual(results, [
    { yesh: 'yesh a' },
    { yesh: 'yesh a!' },
    { yesh: 'yesh b' }
  ])
})
