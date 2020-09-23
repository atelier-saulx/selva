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

test.serial('$language', async t => {
  const client = connect({ port: port })

  await setDataSet(client)

  t.deepEqual(
    await client.get({
      $language: 'en',
      $id: 'mo2001ASpaceOdyssey',
      title: true
    }),
    { title: '2001: A Space Odyssey' }
  )

  t.deepEqual(
    await client.get({
      $language: 'nl',
      $id: 'mo2001ASpaceOdyssey',
      title: true
    }),
    { title: '2001: Een zwerftocht in de ruimte' }
  )

  await client.destroy()
})
