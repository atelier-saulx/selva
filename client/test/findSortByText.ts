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
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          value: { type: 'number' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          fun: { type: 'set', items: { type: 'string' } },
          related: { type: 'references' },
          value: { type: 'number' },
          status: { type: 'number' },
        },
      },
    },
  })

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - sort by text', async (t) => {
  // simple nested - single query
  const client = connect({ port }, { loglevel: 'info' })
  const globMatches = []
  const leaguesSet = []
  for (let i = 0; i < 10; i++) {
    const matches = []
    for (let j = 0; j < 10; j++) {
      const match = {
        $id: await client.id({ type: 'match' }),
        $language: 'en',
        type: 'match',
        name: 'match' + j,
        title: 'match' + j,
        value: Number(i + '.' + j),
        related: globMatches.map((v) => v.$id),
      }
      matches.push(match)
      globMatches.push(match)
    }
    leaguesSet.push({
      type: 'league',
      name: 'league' + i,
      value: i,
      children: matches,
    })
  }
  await Promise.all(leaguesSet.map((v) => client.set(v)))

  const result = await client.get({
    $id: 'root',
    $language: 'en',
    children: {
      id: true,
      title: true,
      $list: {
        $sort: {
          $field: 'name',
          $order: 'asc',
        },
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'match',
          },
        },
      },
    },
  })

  for (let i = 0; i < result.children.length; i++) {
    const idx = Math.floor(i / 10)
    t.deepEqualIgnoreOrder(result.children[i].title, `match${idx}`)
  }

  const result2 = await client.get({
    $id: 'root',
    $language: 'en',
    children: {
      id: true,
      title: true,
      $list: {
        $sort: {
          $field: 'title',
          $order: 'asc',
        },
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'match',
          },
        },
      },
    },
  })

  for (let i = 0; i < result2.children.length; i++) {
    const idx = Math.floor(i / 10)
    t.deepEqualIgnoreOrder(result2.children[i].title, `match${idx}`)
  }

  const result3 = await client.get({
    $id: 'root',
    $language: 'en',
    children: {
      id: true,
      title: true,
      $list: {
        $sort: {
          $field: 'title',
        },
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '=',
            $value: 'match',
          },
        },
      },
    },
  })
  t.truthy(result3)

  await client.destroy()
})

test.serial('sort by text with missing field values', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })
  await client.set({
    type: 'match',
    title: { en: 'abc' },
  })
  await client.set({
    type: 'match',
    title: { en: 'mazzzz' },
  })
  await client.set({
    type: 'match',
  })
  await client.set({
    type: 'match',
    title: { en: '4' },
  })
  await client.set({
    type: 'match',
  })

  t.deepEqual(
    await client.get({
      $language: 'en',
      children: {
        title: true,
        $list: {
          $sort: {
            $field: 'title',
            $order: 'asc',
          },
        },
      },
    }),
    { children: [ { title: '4' }, { title: 'abc' }, { title: 'mazzzz' }, {}, {} ] }
  )

  t.deepEqual(
    await client.get({
      $language: 'en',
      children: {
        title: true,
        $list: {
          $sort: {
            $field: 'title',
            $order: 'asc',
          },
          $offset: 1,
          $limit: 1,
        },
      },
    }),
    { children: [ { title: 'abc' } ] }
  )

  await client.destroy()
})
