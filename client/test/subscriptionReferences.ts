import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'
import './assertions'

let srv
let port: number
test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string' },
          matches: { type: 'references' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          matchType: { type: 'string' },
          date: { type: 'number' },
          completedAt: { type: 'number' },
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

test.serial('add new reference', async (t) => {
  const client = connect({ port })

  const league = await client.set({
    type: 'league',
    name: 'Best',
  })

  await client.set({
    $id: league,
    matches: {
      $add: [
        {
          $id: 'ma1',
          type: 'match',
          matchType: 'interesting',
          date: 1,
        },
      ],
    },
  })

  let res: any
  const obs = client.observe({
    $id: league,
    ongoing: {
      id: true,
      $list: {
        $find: {
          $traverse: 'matches',
          $filter: {
            $field: 'matchType',
            $operator: '=',
            $value: 'interesting',
            $and: {
              $field: 'completedAt',
              $operator: 'notExists',
            },
          },
        },
      }
    },
  })
  obs.subscribe((v) => res = v)

  await wait(100)
  await client.set({
    $id: league,
    matches: {
      $add: [
        {
          $id: 'ma2',
          type: 'match',
          matchType: 'interesting',
          date: 2,
        },
      ],
    },
  })
  await wait(100)
  await client.set({
    $id: league,
    matches: {
      $add: [
        {
          $id: 'ma3',
          date: 2,
          matchType: 'interesting',
          completedAt: 3,
        }
      ]
    },
  })
  await wait(100)
  await client.set({
    $id: 'ma3',
    completedAt: { $delete: true },
  })
  await wait(100)

  //const subs = await client.redis.selva_subscriptions_list('___selva_hierarchy')
  //console.log(subs)
  //console.log(await client.redis.selva_subscriptions_debug('___selva_hierarchy', subs[0]))
  //console.log('ma1', await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'ma1'))
  //console.log('ma2', await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'ma2'))
  //console.log('ma3', await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'ma3'))

  t.deepEqual(res, { ongoing: [ { id: 'ma1' }, { id: 'ma2' }, { id: 'ma3' } ] })

  await client.destroy()
})
