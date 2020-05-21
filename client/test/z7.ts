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
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial(
  'when using parents.$add empty, root should still be added in ancestors (low prio)',
  async t => {
    const client = connect({ port }, { loglevel: 'info' })

    await client.updateSchema({
      types: {
        sport: {
          prefix: 'sp'
        }
      }
    })

    await client.set({
      type: 'sport',
      $id: 'sp1',
      parents: {
        $add: []
      }
    })

    t.deepEqualIgnoreOrder(await client.get({ $id: 'sp1', ancestors: true }), {
      ancestors: ['root']
    })
  }
)

test.serial(
  'ancestors of descendants are updated correct after parent is removed (high prio)',
  async t => {
    const client = connect({ port }, { loglevel: 'info' })

    await client.updateSchema({
      types: {
        sport: {
          prefix: 'sp'
        }
      }
    })

    await client.set({
      type: 'sport',
      $id: 'sp1'
    })

    await client.set({
      type: 'sport',
      $id: 'sp2',
      parents: {
        $add: ['sp1']
      }
    })

    t.deepEqualIgnoreOrder(await client.get({ $id: 'sp2', ancestors: true }), {
      ancestors: ['root', 'sp1']
    })

    await client.delete({
      $id: 'sp1'
    })

    t.deepEqualIgnoreOrder(await client.get({ $id: 'sp2', ancestors: true }), {
      ancestors: ['root']
    })
  }
)
