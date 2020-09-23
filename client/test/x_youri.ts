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
  const client = connect({ port })
  await client.updateSchema({
    languages: ['en'],
    types: {
      sport: {
        prefix: 'sp',
        fields: {
          title: { type: 'text' }
        }
      },
      team: {
        prefix: 'te',
        fields: {
          title: { type: 'text' }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          homeTeam: { type: 'string' },
          awayTeam: { type: 'string' }
        }
      }
    }
  })
  await client.destroy()
})

test.after(async t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscription list', async t => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    $language: 'en',
    type: 'sport',
    title: 'football'
  })

  await client.set({
    $language: 'en',
    type: 'match',
    title: 'football',
    homeTeam: await client.set({
      $language: 'en',
      type: 'team',
      title: 'home team'
    }),
    awayTeam: await client.set({
      $language: 'en',
      type: 'team',
      title: 'away team'
    })
  })

  const obs = client.observe({
    $id: 'root',
    children: {
      teams: [
        {
          $id: { $field: 'homeTeam' },
          title: true
        },
        {
          $id: { $field: 'awayTeam' },
          title: true
        }
      ],
      $list: {
        $limit: 100,
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'sport'
          }
        }
      }
    }
  })

  t.plan(1)
  obs.subscribe(res => {
    t.pass('should still fire even though sport does not have teams')
  })

  await wait(1000)

  await client.destroy()
})
