import test from 'ava'
import { connect } from '@saulx/selva'
import { start } from '@saulx/selva-server'
import '../assertions'
import { wait } from '../assertions'
// @ts-ignore suppressing module can only be default-imported using the 'esModuleInterop' flag
import getPort from 'get-port' 

import { schema } from './_schema'
import { setDataSet } from './_dataSet'

let srv
let port

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  
  await wait(500)
  
  const client = connect({ port: port }, { loglevel: 'info' })
  await client.updateSchema(schema)
  
  await client.destroy()
})

test.serial('$any', async t => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await setDataSet(client)

  const result = await client.get({
    $id: 'peCharltonHeston',
    $all: true
  })

  t.true([
    'name',
    'born',
    'died'
  ].every(key => Object.keys(result).includes(key)))
})

test.serial('$value', async t => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await setDataSet(client)

  const result = await client.get({
    $id: 'mo2001ASpaceOdyssey',
    title: { $value: '2001' }
  })

  t.true(result.title === '2001')
})

test.serial('$default', async t => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await setDataSet(client)

  t.deepEqual(await client.get({
    $id: 'peCharltonHeston',
    died: { $default: '---' }
  }), { died: 2008 })
  
  t.deepEqual(await client.get({
    $id: 'peLeighTaylorYoung',
    died: { $default: '---' }
  }), { died: '---' })
})
