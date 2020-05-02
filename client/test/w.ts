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
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
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
        $inherit: true
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

  const b = await client.get({
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

  t.deepEqual(a, { children: [{ rando: 'rando!' }] })
  t.deepEqual(a, b)
})
