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
    rootType: {
      fields: {
        menu: { type: 'references' }
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

test.serial('inherit references $list', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  const res = await client.get({
    $id: 'root',
    menu: {
      id: true,
      $list: {
        $inherit: true
      }
    }
  })
  t.deepEqualIgnoreOrder(res, { $isNull: true, menu: [] })
  await client.destroy()
})
