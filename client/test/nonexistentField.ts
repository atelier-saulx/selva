import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
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
      user: {
        prefix: 'us',
        fields: {
          name: {
            type: 'string',
          }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('invalid filter should not return result', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    $language: 'en',
    type: 'user',
    name: 'Me me meeee'
  })

  const result = await client.get({
    $language: 'en',
    users: {
      id: true,
      name: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'user'
            },
            {
              $field: 'nonexistent',
              $operator: '=',
              $value: 'something'
            }
          ]
        }
      }
    }
  })
  t.is(result.users.length, 0)
})
