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
    save: true,
  })
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de'],
    types: {
      thing: {
        fields: {
          pageRef: { type: 'reference' },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('do pages with refs', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await wait(100)
  const a = await client.set({
    type: 'thing',
  })
  await wait(100)
  let n = 2
  while (n--) {
    const b = await client.set({
      type: 'thing',
    })
    const sub = client
      .observe({
        $language: 'en',
        $id: b,
        pageRef: {
          id: true,
        },
        // this was the original syntax
        // question: {
        //   $field: 'pageRef',
        //   id: true,
        // },
      })
      .subscribe((res) => {
        // console.log(2, res)
      })
    await wait(100)
    await client.set({
      $id: b,
      pageRef: a,
    })
    await wait(100)
    await client.delete({
      $id: b,
    })
    await wait(100)
    sub.unsubscribe()
    await wait(100)
  }

  await wait(2e3)

  t.pass()

  await client.destroy()
  await client.destroy()
})
