import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'
import { dumpDb } from './assertions'

let srv
let port: number

test.before(async t => {
  port = await getPort()
  srv = await start({
    port
  })
  await new Promise((resolve, _reject) => {
    setTimeout(resolve, 100)
  })

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en'],
    types: {
      lekkerType: {
        prefix: 'vi',
        fields: {
          thing: { type: 'set', items: { type: 'string' } },
          ding: {
            type: 'object',
            properties: {
              dong: { type: 'set', items: { type: 'string' } },
              dung: { type: 'number' },
              dang: {
                type: 'object',
                properties: {
                  dung: { type: 'number' },
                  dunk: { type: 'string' }
                }
              },
              dunk: {
                type: 'object',
                properties: {
                  ding: { type: 'number' },
                  dong: { type: 'number' }
                }
              }
            }
          },
          dong: { type: 'json' },
          dingdongs: { type: 'array', items: { type: 'string' } },
          refs: { type: 'references' },
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      club: {
        prefix: 'cl',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json'
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          description: { type: 'text' }
        }
      }
    }
  })

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('set alias and get by $alias', async t => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    aliases: 'nice_match',
    type: 'match',
    title: { en: 'yesh' }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true
    }),
    {
      id: match1,
      title: 'yesh',
      aliases: ['nice_match']
    }
  )

  t.deepEqualIgnoreOrder(await client.redis.hgetall('___selva_aliases'), {
    nice_match: match1
  })

  t.deepEqualIgnoreOrder(await client.redis.smembers(match1 + '.aliases'), [
    'nice_match'
  ])

  const match2 = await client.set({
    aliases: ['nice_match', 'very_nice_match'],
    type: 'match',
    title: { en: 'yesh2' }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true
    }),
    {
      id: match2,
      title: 'yesh2',
      aliases: ['nice_match', 'very_nice_match']
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'very_nice_match',
      id: true,
      title: true
    }),
    {
      id: match2,
      title: 'yesh2'
    }
  )

  t.deepEqualIgnoreOrder(await client.redis.hgetall('___selva_aliases'), {
    nice_match: match2,
    very_nice_match: match2
  })

  t.deepEqualIgnoreOrder(await client.redis.smembers(match2 + '.aliases'), [
    'nice_match',
    'very_nice_match'
  ])

  t.deepEqualIgnoreOrder(await client.redis.smembers(match1 + '.aliases'), [])

  await client.set({
    $id: match1,
    aliases: { $add: ['ok_match'] }
  })

  await client.set({
    $id: match2,
    aliases: { $delete: ['very_nice_match'] }
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true
    }),
    {
      id: match2,
      title: 'yesh2',
      aliases: ['nice_match']
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'ok_match',
      id: true,
      title: true,
      aliases: true
    }),
    {
      id: match1,
      title: 'yesh',
      aliases: ['ok_match']
    }
  )

  t.deepEqualIgnoreOrder(await client.redis.hgetall('___selva_aliases'), {
    ok_match: match1,
    nice_match: match2
  })

  t.deepEqualIgnoreOrder(await client.redis.smembers(match2 + '.aliases'), [
    'nice_match'
  ])

  t.deepEqualIgnoreOrder(await client.redis.smembers(match1 + '.aliases'), [
    'ok_match'
  ])

  await client.delete('root')
  client.destroy()
})
