import { join as pathJoin } from 'path'
import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait, removeDump } from './assertions'
import getPort from 'get-port'

let srv
let port: number
const dir = pathJoin(process.cwd(), 'tmp', 'z19-youri-ref-test')

test.before(async (t) => {
  removeDump(dir)()
  port = await getPort()
  srv = await start({
    port,
    save: true,
    dir,
  })
  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de'],
    types: {
      thing: {
        fields: {
          fileRef: { type: 'reference' },
        },
      },
      file: {
        prefix: 'fi',
        fields: {
          src: {
            type: 'string',
          },
          name: {
            type: 'string',
          },
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
  await t.connectionsAreEmpty()
  removeDump(dir)()
})

test.serial('update refs in descendants query', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await wait(100)

  const thing = await client.set({
    type: 'thing',
  })

  const parent = await client.set({
    type: 'thing',
    // @ts-ignore
    children: [thing],
  })

  await wait(100)
  let lastSubResult
  const sub = client
    .observe({
      $language: 'en',
      $id: parent,
      descendants: {
        fileRef: {
          id: true,
          name: true,
          src: true,
        },
        $list: true,
      },
    })
    .subscribe((res) => {
      lastSubResult = JSON.stringify(res, null, 2)
      console.log('sub fires:', lastSubResult)
    })

  await wait(100)

  const file = await client.set({
    type: 'file',
    name: 'success!',
  })

  console.log('add the file ref to thing', file)

  await client.set({
    $id: thing,
    fileRef: file,
  })

  await wait(100)

  console.log('add src to file')

  await client.set({
    $id: file,
    src: 'https://google.com',
  })

  await wait(1000)

  sub.unsubscribe()

  await wait(100)

  const getResult = JSON.stringify(
    await client.get({
      $language: 'en',
      $id: parent,
      descendants: {
        fileRef: {
          id: true,
          name: true,
          src: true,
        },
        $list: true,
      },
    }),
    null,
    2
  )

  console.log('get result is:', getResult)

  t.is(lastSubResult, getResult)

  await client.destroy()
})
