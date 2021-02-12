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

test.serial('changing alias to another node fires subscription', async (t) => {
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: { yesh: { type: 'string' }, no: { type: 'string' } },
    },
    types: {
      yeshType: {
        prefix: 'ye',
        fields: {
          yesh: { type: 'string' },
        },
      },
    },
  })

  t.plan(2)

  await client.set({
    $id: 'yebba',
    yesh: 'pretty nice',
    aliases: { $add: 'hello-friend' },
  })

  const observable = client.observe({
    $alias: 'hello-friend',
    yesh: true,
  })

  let o1counter = 0
  const sub = observable.subscribe((d) => {
    if (o1counter === 0) {
      // gets start event
      t.is(d.yesh, 'pretty nice')
    } else if (o1counter === 1) {
      // gets update event
      t.deepEqualIgnoreOrder(d, { yesh: 'extra nice' })
    } else {
      // doesn't get any more events
      t.fail()
    }
    o1counter++
  })

  await wait(500 * 2)

  const thing = await client.set({
    $id: 'yebbe',
    yesh: 'extra nice',
    aliases: { $add: 'hello-friend' },
  })

  await wait(500 * 2)

  sub.unsubscribe()

  await wait(500 * 2)

  await client.delete('root')

  await wait(1000)

  await client.destroy()
})
