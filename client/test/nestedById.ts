import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'

import getPort from 'get-port'

test('Create fields from $ids', async t => {
  const port = await getPort()
  const server = await start({ port })

  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  await client.updateSchema({
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'string'
          }
        }
      }
    }
  })

  await client.set({
    $id: 'ma1',
    title: 'HELLO'
  })

  await client.set({
    $id: 'ma2',
    title: 'BYE'
  })

  await client.set({
    $id: 'ma3',
    // $alias: { $add: 'flap' },
    title: 'NUX'
  })

  const item = await client.get({
    flurpy: {
      $id: 'ma2',
      title: true
    }
  })

  console.log(item)

  server.destroy()
})
