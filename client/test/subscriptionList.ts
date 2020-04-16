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
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          name: { type: 'string', search: { type: ['TAG'] } },
          value: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          status: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } },
          date: { type: 'number', search: { type: ['NUMERIC', 'SORTABLE'] } }
        }
      }
    }
  })
})

test.after(async _t => {
  const client = connect({ port })
  const d = Date.now()
  await client.delete('root')
  console.log('removed', Date.now() - d, 'ms')
  await client.destroy()
  await srv.destroy()
})

test.serial('subscription list', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const matches = []

  await wait(500)

  for (let i = 0; i < 10; i++) {
    matches.push({
      $id: await client.id({ type: 'match' }),
      name: 'match ' + i,
      type: 'match',
      value: i,
      status: i < 5 ? 100 : 300
    })
  }

  await Promise.all(matches.map(v => client.set(v)))

  await wait(500)

  // without sort
  const flap = await client.get({
    $includeMeta: true,
    children: {
      name: true,
      id: true,
      $list: {}
    }
  })

  t.is(Object.keys(flap.$meta.query[0].ids).length, 9)

  const ff = await client.get({
    $includeMeta: true,
    children: {
      name: true,
      id: true,
      $list: {
        $find: {
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'match' // bit nicer error habndling if you do something weird here
          }
        }
      }
    }
  })

  t.is(Object.keys(ff.$meta.query[0].ids).length, 9)

  await wait()

  const obs = await client.observe({
    children: {
      name: true,
      id: true,
      $list: {}
    }
  })
  let cnt = 0
  const sub = obs.subscribe(d => {
    cnt++
  })

  await wait(1000)
  t.is(cnt, 1)

  client.set({
    $id: matches[0].$id,
    name: 'FLURP!'
  })

  await wait(1000)
  t.is(cnt, 2)
  sub.unsubscribe()

  const obs2 = await client.observe({
    $language: 'en', // need this in my meta query
    title: true,
    children: {
      name: true,
      title: true,
      type: true,
      $list: {}
    }
  })

  const obs3 = await client.observe({
    $language: 'en', // need this in my meta query, also need to use schema for this (adding lang field to text fields)
    title: true,
    items: {
      name: true,
      title: true,
      type: true,
      $list: {
        $find: {
          $traverse: 'children'
        }
      }
    }
  })

  let cnt2 = 0
  let cnt3 = 0
  const sub2 = obs2.subscribe(d => {
    cnt2++
  })

  const sub3 = obs3.subscribe(d => {
    cnt3++
  })

  await wait(2000)

  client.set({
    $id: matches[0].$id,
    title: { en: 'Flapdrol' }
  })

  await wait(2000)
  sub2.unsubscribe()
  sub3.unsubscribe()
  t.is(cnt3, 2)
  t.is(cnt2, 2)
})
