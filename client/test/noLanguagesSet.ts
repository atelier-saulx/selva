import test from 'ava'
import { connect } from '../src/index'
import { SelvaServer, start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'

let srv: SelvaServer
let port: number
test.beforeEach(async (_t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  const client = connect({ port })
  await client.updateSchema({
    types: {
      match: {
        fields: {
          title: {
            type: 'string',
          },
          yeye: {
            type: 'text',
          },
        },
      },
    },
  })
  await client.destroy()
})

test.afterEach(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('get with $language and no languages in schema', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  const id = await client.set({
    type: 'match',
    title: 'wawa',
  })

  const error = await t.throwsAsync(
    client.get({
      $id: id,
      $language: 'en',
      title: true,
    })
  )
  t.regex(error.message, /no languages set/)
})

test.serial('set with $language and no languages in schema', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const error = await t.throwsAsync(
    client.set({
      $language: 'en',
      type: 'match',
      title: 'wawa',
    })
  )
  t.regex(error.message, /no languages set/)
})

test.serial(
  'get with $language and languages set should not fail',
  async (t) => {
    const client = connect({ port }, { loglevel: 'info' })

    await client.updateSchema({
      languages: ['en'],
    })

    const id = await client.set({
      type: 'match',
      title: 'wawa',
    })

    await t.notThrowsAsync(
      client.get({
        $id: id,
        $language: 'en',
        title: true,
      })
    )
  }
)

test.serial(
  'set with $language and no languages set should not fail',
  async (t) => {
    const client = connect({ port }, { loglevel: 'info' })

    await client.updateSchema({
      languages: ['en'],
    })

    await t.notThrowsAsync(
      client.set({
        $language: 'en',
        type: 'match',
        title: 'wawa',
      })
    )
  }
)
