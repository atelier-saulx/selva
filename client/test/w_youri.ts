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

test.serial('subscription list', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    type: 'sport',
    rando: 'rando!',
    children: [
      {
        type: 'match'
      }
    ]
  })

  const a = await client.get({
    $id: 'root',
    children: {
      rando: {
        $inherit: { $type: 'sport' }
      },
      $list: {
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'match'
          }
        }
      }
    }
  })

  // const b = await client.get({
  //   $id: 'root',
  //   children: {
  //     rando: {
  //       $inherit: { $type: 'sport' }
  //     },
  //     $list: {
  //       $limit: 100,
  //       $find: {
  //         $traverse: 'descendants',
  //         $filter: {
  //           $field: 'type',
  //           $operator: '=',
  //           $value: 'match'
  //         }
  //       }
  //     }
  //   }
  // })

  t.deepEqual(a, { children: [{ rando: 'rando!' }] })
  // t.deepEqual(a, b)

  await client.destroy()
})
