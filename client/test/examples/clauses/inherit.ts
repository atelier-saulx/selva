import test from 'ava'
import { connect } from '@saulx/selva'
import { start, SelvaServer } from '@saulx/selva-server'
import '../../assertions'
import { wait } from '../../assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port' 

import { schema } from '../_schema'
import { setDataSet } from '../_dataSet'

let srv:SelvaServer
let port

test.before(async t => {
  port = await getPort()
  srv = await start({ port })
  await wait(500)
  const client = connect({ port: port })
  await client.updateSchema(schema)
  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('$inherit', async t => {
  const client = connect({ port: port })

  await setDataSet(client)

  const result = await client.get({
    $id: 'moSoylentGreen',
    icon: { $inherit: true },
  })

  t.true(result.icon === 'scifi.png')
})
