import test from 'ava'
import { connect } from '@saulx/selva'
import { start } from '@saulx/selva-server'
import '../../assertions'
import { wait } from '../../assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port'

import { schema } from '../_schema'
import { setDataSet } from '../_dataSet'

let srv
let port

test.before(async (t) => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port: port })
  await client.updateSchema(schema)
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('$any', async (t) => {
  const client = connect({ port: port })

  await setDataSet(client)

  const result = await client.get({
    $id: 'peCharltonHeston',
    $all: true,
  })

  t.true(
    ['name', 'born', 'died'].every((key) => Object.keys(result).includes(key))
  )
  await client.destroy()
})

test.serial('$all with blacklist', async (t) => {
  const client = connect({ port: port })

  await setDataSet(client)

  const result = await client.get({
    $id: 'peCharltonHeston',
    $all: true,
    died: false,
  })

  t.false(Object.keys(result).includes('died'))
  await client.destroy()
})
