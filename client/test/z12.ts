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
    types: {
      match: {
        fields: {
          published: {
            type: 'boolean'
          }
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

test.serial('correct validation for booleans', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.set({
    type: 'match',
    published: true
  })

  try {
    const res = await client.get({
      descendants: {
        type: true,
        $list: {
          $find: {
            $filter: {
              $field: 'published',
              $operator: '!=',
              $value: false
            }
          }
        }
      }
    })

    t.deepEqualIgnoreOrder(res, {
      descendants: [{ type: 'match' }]
    })
  } catch (e) {
    console.error(e)
    t.fail()
  }
})
