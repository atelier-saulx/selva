import test from 'ava'
import './assertions'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { dumpDb } from './assertions'
import getPort from 'get-port'
import { wait } from '../src/util'

const DEFAULT_HIERARCHY = '___selva_hierarchy';

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
})
test.beforeEach(async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'nl', 'de'],
    rootType: {
      fields: { value: { type: 'number' }, title: { type: 'text' } }
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: {
            type: 'text'
          },
          createdAt: { type: 'timestamp' },
          value: { type: 'number' }
        }
      },
      league: {
        prefix: 'cu',
        fields: {
          title: {
            type: 'text'
          },
          createdAt: { type: 'number' }
        }
      },
      person: {
        prefix: 'pe',
        fields: {
          title: {
            type: 'text'
          },
          createdAt: { type: 'timestamp' },
          updatedAt: { type: 'timestamp' }
        }
      },
      someTestThing: {
        prefix: 'vi',
        fields: {
          title: {
            type: 'text'
          },
          value: {
            type: 'number'
          }
        }
      },
      otherTestThing: {
        prefix: 'ar',
        fields: {
          title: {
            type: 'text'
          },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' }
            }
          }
        }
      }
    }
  })

  // A small delay is needed after setting the schema
  await new Promise(r => setTimeout(r, 100))

  await client.destroy()
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.serial('can delete root', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match'
  })

  const root = await client.set({
    $id: 'root',
    value: 9001
  })

  t.deepEqual(root, 'root')
  t.deepEqual(await client.redis.hget('root', 'value'), '9001')
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [match])

  await client.delete('root')
  t.deepEqual(await dumpDb(client), [])

  await client.destroy()
})

test.serial('can delete a field', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match',
    value: 9002
  })

  await client.set({
    $id: 'root',
    value: 9001
  })

  t.deepEqual(await client.redis.hget('root', 'value'), '9001')
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [match])
  t.deepEqual(await client.redis.hget(match, 'value'), '9002')

  await client.delete({
    $id: 'root',
    value: true
  })

  t.deepEqual(await client.redis.hexists('root', 'value'), 0)
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [match])
  t.deepEqual(await client.redis.hget(match, 'value'), '9002')

  await client.delete({
    $id: match,
    value: true
  })

  t.deepEqual(await client.redis.hexists('root', 'value'), 0)
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [match])
  t.deepEqual(await client.redis.hexists(match, 'value'), 0)

  await client.delete({
    $id: 'root',
    children: true
  })

  t.deepEqual(await client.redis.hexists('root', 'value'), 0)
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [])
  t.deepEqual(await client.redis.hexists(match, 'value'), 0)

  await client.delete('root')
  await client.delete(match)
  await client.destroy()
})

test.serial('can delete all but some fields', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match',
    value: 9002,
    title: { en: 'yes yeesh', de: 'ja ja' }
  })

  await client.set({
    $id: 'root',
    value: 9001,
    title: { en: 'no noes', de: 'nein nein' }
  })

  t.deepEqual(await client.redis.hget('root', 'value'), '9001')
  t.deepEqual(await client.redis.hget('root', 'title.en'), 'no noes')
  t.deepEqual(await client.redis.hget('root', 'title.de'), 'nein nein')
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [match])
  t.deepEqual(await client.redis.hget(match, 'value'), '9002')
  t.deepEqual(await client.redis.hget(match, 'title.en'), 'yes yeesh')
  t.deepEqual(await client.redis.hget(match, 'title.de'), 'ja ja')

  await client.delete({
    $id: 'root',
    title: {
      en: false
    }
  })

  t.deepEqual(await client.redis.hexists('root', 'value'), 1)
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [match])
  t.deepEqual(await client.redis.hget('root', 'title.en'), 'no noes')
  t.deepEqual(await client.redis.hexists('root', 'title.de'), 0)

  await client.delete({
    $id: match,
    title: {
      en: false
    }
  })

  t.deepEqual(await client.redis.hexists(match, 'value'), 1)
  t.deepEqual(await client.redis.hget(match, 'title.en'), 'yes yeesh')
  t.deepEqual(await client.redis.hexists(match, 'title.de'), 0)

  await client.delete({
    $id: 'root',
    title: {
      en: true
    },
    children: true
  })

  t.deepEqual(await client.redis.hexists('root', 'title.en'), 0)
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [])

  await client.delete('root')
  await client.delete(match)
  await client.destroy()
})

test.serial('can delete a field when only nested specified', async t => {
  const client = connect(
    {
      port
    },
    { loglevel: 'info' }
  )

  const match = await client.set({
    type: 'match',
    title: {
      en: 'yes text',
      de: 'ja text'
    }
  })

  await client.set({
    $id: 'root',
    value: 9001
  })

  t.deepEqual(await client.redis.hget('root', 'value'), '9001')
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [match])
  t.deepEqual(await client.redis.hget(match, 'title.de'), 'ja text')
  t.deepEqual(await client.redis.hget(match, 'title.en'), 'yes text')

  await client.delete({
    $id: match,
    title: {
      en: true
    }
  })

  t.deepEqual(await client.redis.hexists(match, 'title.en'), 0)
  t.deepEqual(await client.redis.hget('root', 'value'), '9001')
  t.deepEqual(await client.redis.selva_hierarchy_children(DEFAULT_HIERARCHY, 'root'), [match])
  t.deepEqual(await client.redis.hget(match, 'title.de'), 'ja text')
  await client.delete('root')
  await client.delete(match)
  await client.destroy()
})

// test.serial.only('can delete child from root', async t => {
//   const client = connect(
//     {
//       port
//     },
//     { loglevel: 'info' }
//   )

//   client
//     .observe({
//       $id: 'root',
//       children: {
//         id: true,
//         title: true,
//         $list: true
//       }
//     })
//     .subscribe(res => console.log('-->', res))

//   const match = await client.set({
//     type: 'match',
//     title: {
//       en: 'yes text',
//       de: 'ja text'
//     }
//   })

//   await client.delete({
//     $id: match
//   })

//   await wait(500)

//   console.log('check me:', await client.get({ $id: 'root', children: true }))

//   await client.destroy()
// })
