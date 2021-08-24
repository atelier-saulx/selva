import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          media: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                src: { type: 'url' },
              },
            },
          },
        },
      },
    },
  })

  // A small delay is needed after setting the schema
  await new Promise((r) => setTimeout(r, 100))

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('should replace array', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const originalMedia = [
    { src: 'http://wawa.com/111' },
    { src: 'http://wawa.com/222' },
    { src: 'http://wawa.com/333' },
  ]

  const lekker = await client.set({
    type: 'lekkerType',
    media: originalMedia,
  })
  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $id: lekker,
      id: true,
      media: true,
    }),
    {
      id: lekker,
      media: originalMedia,
    }
  )

  await client.set({
    $id: lekker,
    media: [{ src: 'http://wawa.com/222' }, { src: 'http://wawa.com/333' }],
  })
  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $id: lekker,
      id: true,
      media: true,
    }),
    {
      id: lekker,
      media: [{ src: 'http://wawa.com/222' }, { src: 'http://wawa.com/333' }],
    }
  )

  await client.set({
    $id: lekker,
    media: [{ src: 'http://wawa.com/444' }],
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $id: lekker,
      id: true,
      media: true,
    }),
    {
      id: lekker,
      media: [{ src: 'http://wawa.com/444' }],
    }
  )

  await client.set({
    $id: lekker,
    media: [],
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $id: lekker,
      id: true,
      media: true,
    }),
    {
      id: lekker,
    }
  )

  await client.delete('root')
  await client.destroy()
})
