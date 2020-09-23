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
      sport: {
        prefix: 'sp',
        fields: {
          rando: { type: 'string' }
        }
      },
      match: {
        prefix: 'ma'
      }
    }
  })
  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('insert works only if object is not defined', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const thing = await client.set({
    $operation: 'insert',
    type: 'sport',
    rando: 'rando!'
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: thing,
      rando: true
    }),
    { rando: 'rando!' }
  )

  const retVal = await client.set({
    $id: thing,
    $operation: 'insert',
    type: 'sport',
    rando: 'rando!!!'
  })

  t.is(retVal, undefined)
  t.deepEqualIgnoreOrder(
    await client.get({
      $id: thing,
      rando: true
    }),
    { rando: 'rando!' }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('update works only if object is defined', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  let retVal = await client.set({
    $id: 'spRando',
    $operation: 'update',
    type: 'sport',
    rando: 'rando!'
  })

  t.is(retVal, undefined)

  await client.set({
    $id: 'spRando',
    $operation: 'insert',
    type: 'sport',
    rando: 'rando!'
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'spRando',
      rando: true
    }),
    { rando: 'rando!' }
  )

  retVal = await client.set({
    $id: 'spRando',
    $operation: 'update',
    type: 'sport',
    rando: 'rando!!!'
  })

  t.is(retVal, 'spRando')
  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'spRando',
      rando: true
    }),
    { rando: 'rando!!!' }
  )

  await client.delete('root')
  await client.destroy()
})
