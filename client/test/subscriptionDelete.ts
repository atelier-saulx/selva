import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number
test.before(async () => {
  port = await getPort()
  srv = await start({
    port,
  })
})

test.after(async (t) => {
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('subscribe and delete', async (t) => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      thing: {
        fields: {
          yesh: { type: 'number' },
        },
      },
    },
  })

  const q = []
  for (let i = 0; i < 10; i++) {
    q.push(
      client.set({
        type: 'thing',
        yesh: i,
      })
    )
  }

  const ids = await Promise.all(q)

  let cnt = 0
  const observable = client.observe({
    $id: 'root',
    things: {
      id: true,
      yesh: true,
      $list: {
        $find: {
          $traverse: 'children', // also desc
          $filter: {
            $operator: '=',
            $value: 'thing',
            $field: 'type',
          },
        },
      },
    },
  })

  const s = observable.subscribe((d) => {
    cnt++
    console.info(d)
  })

  await wait(1000)

  await client.set({ type: 'thing', yesh: 2 })

  await wait(1000)

  await client.delete({ $id: ids[0] })

  await wait(1000)

  t.is(cnt, 3)

  s.unsubscribe()

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})
