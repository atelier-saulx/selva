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
})

test.after(async t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('root is searchable', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    rootType: {
      fields: {
        title: {
          type: 'text',
          search: { type: ['TEXT-LANGUAGE-SUG'] }
        }
      }
    },
    types: {
      sport: {
        prefix: 'sp',
        fields: {
          title: {
            type: 'text',
            search: { type: ['TEXT-LANGUAGE-SUG'] }
          }
        }
      }
    }
  })

  await client.set({
    $language: 'en',
    $id: 'root',
    title: 'yesh'
  })

  await client.set({
    $language: 'en',
    type: 'sport',
    $id: 'sp1',
    title: 'yesh'
  })

  await client.set({
    $language: 'en',
    type: 'sport',
    $id: 'sp2',
    title: 'yesh',
    parents: ['sp1']
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'sp2',
      $language: 'en',
      results: {
        id: true,
        title: true,
        $list: {
          $find: {
            $filter: [
              {
                $field: 'title',
                $operator: '=',
                $value: 'yesh'
              }
            ],
            $traverse: 'ancestors'
          }
        }
      }
    }),
    {
      results: [
        { id: 'sp1', title: 'yesh' },
        { id: 'root', title: 'yesh' }
      ]
    }
  )

  await client.destroy()
})

test.serial('root is searchable 2', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.updateSchema({
    languages: ['en'],
    types: {
      sport: {
        prefix: 'sp',
        fields: {
          default: {
            type: 'object',
            properties: {
              rando: { type: 'text' }
            }
          }
        }
      }
    }
  })

  try {
    await client.set({
      $id: 'sp1',
      default: {
        rando: {
          en: 'jonko'
        }
      }
    })
    t.pass()
  } catch (e) {
    console.error(e)
    t.fail()
  }

  await client.destroy()
})
