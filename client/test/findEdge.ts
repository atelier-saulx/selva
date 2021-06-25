
import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port: number

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      myclass: {
        prefix: 'cl',
        fields: {
          title: { type: 'text' },
          num: { type: 'number' },
          subclasses: { type: 'references' },
        },
      },
    },
  })

  const firstId = await client.set({
    type: 'myclass',
    title: { en: 'First class' }
  })

  const addSub = (n, i) => i
    ? [
      {
        type: 'myclass',
        title: { en: `Subclass ${n}${i}` },
        num: n * i,
        parents: [],
        subclasses: addSub(n, i - 1),
      }
    ]
    : []
  let ids = [];
  for (let i = 0; i < 5; i++) {
    await client.set({
      $id: firstId,
      subclasses: {
        $add: {
          type: 'myclass',
          title: { en: `Subclass ${i}` },
          num: i,
          parents: [],
          subclasses: addSub(i, 100),
        }
      }
    })
  }

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find', async (t) => {
  // simple nested - single query

  try {
    const client = connect({ port }, { loglevel: 'info' })

    await wait(2e3)

    const firstId = (await client.get({
      $id: 'root',
      children: true,
    })).children[0]
    t.assert(firstId)
    const res = await client.get({
      $id: firstId,
      items: {
        id: true,
        title: true,
        $list: {
          $find: {
            $traverse: 'subclasses',
            $recursive: true,
            $filter: {
              $field: 'num',
              $operator: '=',
              $value: 180,
            },
          },
        },
      },
    })
    t.deepEqual(res.items.length, 3)

    await client.destroy()
  } catch (err) {
    console.error(err)
  }
})
