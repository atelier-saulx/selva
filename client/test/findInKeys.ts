import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait, worker } from './assertions'
import getPort from 'get-port'
import chalk from 'chalk'

let srv
let port: number
test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en'],
    types: {
      glurp: {
        prefix: 'gl',
        fields: {
          levelCnt: { type: 'number' },
          title: { type: 'string' }
        }
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

test.serial('get in keys result', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'root',
    children: [
      {
        type: 'glurp',
        id: 'gl0',
        title: 'cookie'
      },
      {
        type: 'glurp',
        id: 'gl1',
        title: 'glurpie pants'
      },
      {
        type: 'glurp',
        id: 'gl2',
        title: 'glurpie pants 2'
      },
      {
        type: 'glurp',
        id: 'gl3',
        title: 'cookie'
      }
    ]
  })

  const gimme = await client.get({
    tilte: true,
    id: true,
    $find: {
      $traverse: ['gl1', 'gl2', 'gl3'],
      $filter: {
        $field: 'title',
        $operator: '=',
        $value: 'cookie'
      }
    }
  })

  console.log({ gimme })

  await wait(1e3)

  await client.destroy()
})
