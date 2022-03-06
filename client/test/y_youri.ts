import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      sport: {
        prefix: 'sp',
        fields: {
          title: { type: 'text' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          published: {
            type: 'boolean',
          },
        },
      },
    },
  })
  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscription list', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const sport = await client.set({
    $language: 'en',
    type: 'sport',
    title: 'football',
  })

  const match = await client.set({
    $language: 'en',
    type: 'match',
    title: 'football match',
    published: true,
    parents: [sport],
  })

  const obs = client.observe({
    $id: match,
    $language: 'en',
    items: {
      title: true,
      $list: {
        $limit: 10,
        $find: {
          $traverse: 'ancestors',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'sport',
          },
          $find: {
            $traverse: 'descendants',
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'published',
                $operator: '=',
                $value: true,
              },
            ],
          },
        },
      },
    },
  })

  t.plan(1)
  obs.subscribe((res) => {
    t.deepEqual(res.items, [{ title: 'football match' }])
  })

  await wait(1000)

  await client.destroy()
})
