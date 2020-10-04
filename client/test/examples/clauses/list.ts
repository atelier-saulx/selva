import test from 'ava'
import { connect } from '@saulx/selva'
import { start, SelvaServer } from '@saulx/selva-server'
import '../../assertions'
import { wait } from '../../assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port'

import { schema } from '../_schema'
import { setDataSet } from '../_dataSet'

let srv: SelvaServer
let port

test.before(async t => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port: port })
  await client.updateSchema(schema)
  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('$list', async t => {
  const client = connect({ port: port })

  await setDataSet(client)

  const result = await client.get({
    $id: 'geScifi',
    $language: 'en',
    children: {
      title: true,
      $list: {}
    }
  })

  t.deepEqual(result.children.length, 3)

  await client.destroy()
})

test.serial('$order', async t => {
  const client = connect({ port: port })

  await setDataSet(client)

  const result = await client.get({
    $id: 'geScifi',
    $language: 'en',
    children: {
      title: true,
      year: true,
      $list: {
        $sort: { $field: 'year', $order: 'asc' }
      }
    }
  })

  t.deepEqual(result.children[0].title, 'Metropolis')

  await client.destroy()
})

// TODO: Unskip this test when $range is fixed
test.serial.skip('$range', async t => {
  const client = connect({ port: port })

  await setDataSet(client)

  const result = await client.get({
    $id: 'geScifi',
    $language: 'en',
    children: {
      title: true,
      year: true,
      $list: {
        $sort: { $field: 'year', $order: 'asc' },
        $range: { $offset: 0, $limit: 2 }
      }
    }
  })

  t.deepEqual(result.children.length, 2)

  await client.destroy()
})
