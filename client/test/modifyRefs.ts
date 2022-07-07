import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        ref: { type: 'reference' },
      },
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          ref: { type: 'reference' },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await wait(100)

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('implicitly created nodes', async (t) => {
  const client = connect({ port })

  await client.set({
    $id: 'root',
    children: ['ma1', 'ma2'],
    ref: 'ma3',
  })
  await client.set({
    $id: 'ma5',
    ref: {
      $id: 'ma6',
      ref: 'ma4',
    },
    children: [
      {
        $id: 'ma7',
      }
    ],
  })

  t.deepEqual(
    await client.get({
      ref: {
        id: true,
        type: true,
      },
      matches: {
        id: true,
        type: true,
        $list: {
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              }
            ],
          }
        },
      },
    }),
    {
      matches: [
        { id: 'ma1', type: 'match' },
        { id: 'ma2', type: 'match' },
        { id: 'ma5', type: 'match' },
        { id: 'ma6', type: 'match' },
        { id: 'ma7', type: 'match' },
      ],
      ref: { id: 'ma3', type: 'match' },
    }
  )

  t.deepEqual(
    await client.get({
      $id: 'ma5',
      id: true,
      type: true,
      children: {
        id: true,
        type: true,
        $list: {
          $find: {
            $traverse: 'children',
          }
        }
      },
      ref: {
        id: true,
        type: true,
      },
    }),
    {
      id: 'ma5',
      type: 'match',
      children: [ { id: 'ma7', type: 'match' } ],
      ref: { id: 'ma6', type: 'match' }
    }
  )

  await client.destroy()
})
