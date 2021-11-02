import test from 'ava'
import { connect } from '@saulx/selva'
import {
  startRegistry,
  startOrigin,
  startSubscriptionManager,
} from '../../server'
import { wait } from './assertions'
import getPort from 'get-port'

test.serial('connection min', async (t) => {
  await wait(2e3)
  const port = await getPort()

  const registry = await startRegistry({ port })

  console.info('go go go client')

  const client = connect({
    port,
  })

  console.info('YES CONNECT IT!')

  await wait(2000)

  console.info('wait is done')
  console.info('\n\n\n----------------------')

  const origin = await startOrigin({
    default: true,
    registry: { port },
    dir: null,
  })

  console.info('update shcema')
  await client.updateSchema({
    types: {
      flap: {
        fields: {
          snurk: { type: 'string' },
        },
      },
    },
  })
  console.info('update schema done')

  const x = await client.set({
    type: 'flap',
    snurk: 'yes!',
  })

  console.info('🦉', x)

  const flap = await client.get({
    $id: x,
    $all: true,
  })

  console.info('SNURXXX', flap)

  const subManager = await startSubscriptionManager({
    registry: { port },
  })

  const unsubsribe = client
    .observe({
      snurks: {
        $all: true,
        $list: {
          $find: {
            $traverse: 'children',
            $filter: {
              $field: 'type',
              $operator: '=',
              $value: 'flap',
            },
          },
        },
      },
    })
    .subscribe((d) => {
      console.info('------------------------>', d)
    })

  // unsubsribe

  await wait(10000)

  console.info('\n\n\n----------------------')
  console.info('REMOVE ALL')

  await registry.destroy()
  await origin.destroy()
  await client.destroy()

  await t.connectionsAreEmpty()
})
