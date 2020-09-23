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
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
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
      $id: 'sp11',
      parents: {
        $add: []
      }
    })

    t.deepEqualIgnoreOrder(await client.get({ $id: 'sp11', ancestors: true }), {
      ancestors: ['root']
    })

    await client.destroy()
  }
)

// test.serial(
//   'ancestors of descendants are updated correct after parent is removed (high prio)',
//   async t => {
//     const client = connect({ port }, { loglevel: 'info' })
//
//     await client.updateSchema({
//       types: {
//         sport: {
//           prefix: 'sp'
//         }
//       }
//     })
//
//     await client.set({
//       type: 'sport',
//       $id: 'sp1'
//     })
//
//     await client.set({
//       type: 'sport',
//       $id: 'sp2',
//       parents: {
//         $add: ['sp1']
//       }
//     })
//
//     t.deepEqualIgnoreOrder(await client.get({ $id: 'sp2', ancestors: true }), {
//       ancestors: ['root', 'sp1']
//     })
//
//     await client.delete({
//       $id: 'sp1'
//     })
//
//     t.deepEqualIgnoreOrder(await client.get({ $id: 'sp2', ancestors: true }), {
//       ancestors: ['root']
//     })
//   }
// )
