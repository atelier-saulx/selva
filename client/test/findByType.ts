import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import { wait } from './assertions'
import getPort from 'get-port'

let srv
let port

test.before(async (t) => {
  port = await getPort()
  srv = await start({
    port,
  })

  await wait(500)
})

test.beforeEach(async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en'],
    types: {
      league: {
        prefix: 'le',
        fields: {
          name: { type: 'string' },
          thing: { type: 'string' },
          matches: { type: 'references' },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          name: { type: 'string' },
          description: { type: 'text' },
          value: {
            type: 'number',
          },
          status: { type: 'number' },
        },
      },
    },
  })

  await wait(100)

  await client.destroy()
})

test.after(async (t) => {
  const client = connect({ port: port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
  await t.connectionsAreEmpty()
})

test.serial('find - by type', async (t) => {
  // simple nested - single query
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.set({
    $id: 'le1',
    type: 'league',
    name: 'league 1',
  })

  await client.set({
    $id: 'ma1',
    parents: ['le1'],
    type: 'match',
    name: 'match 1',
    value: 1,
  })

  await client.set({
    $id: 'ma2',
    parents: ['ma1'],
    type: 'match',
    name: 'match 2',
    value: 2,
  })

  await client.set({
    $id: 'ma3',
    parents: ['ma1'],
    type: 'match',
    name: 'match 3',
  })

  await client.set({
    $id: 'ma4',
    parents: ['le1'],
    type: 'match',
    name: 'match 4',
    value: 12312,
  })

  await client.set({
    $id: 'le1',
    matches: ['ma1', 'ma2', 'ma3'],
  })

  t.deepEqual(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        nonsense: { $default: 'yes' },
        $list: {
          $find: {
            $recursive: true,
            $traverse: {
              root: 'children',
              league: 'children',
              $any: false,
            },
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'value',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    }),
    {
      id: 'root',
      items: [
        { name: 'match 1', nonsense: 'yes' },
        { name: 'match 4', nonsense: 'yes' },
        // { name: 'match 2', nonsense: 'yes' },
      ],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        nonsense: { $default: 'yes' },
        $list: {
          $find: {
            $recursive: true,
            $traverse: {
              root: 'children',
              league: { $first: ['matches', 'children'] },
              $any: false,
            },
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'value',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    }),
    {
      id: 'root',
      items: [
        { name: 'match 1', nonsense: 'yes' },
        { name: 'match 2', nonsense: 'yes' },
      ],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: 'root',
      id: true,
      items: {
        name: true,
        nonsense: { $default: 'yes' },
        $list: {
          $find: {
            $recursive: true,
            $traverse: {
              root: 'children',
              league: { $all: ['matches', 'children'] },
              $any: false,
            },
            $filter: [
              {
                $field: 'type',
                $operator: '=',
                $value: 'match',
              },
              {
                $field: 'value',
                $operator: 'exists',
              },
            ],
          },
        },
      },
    }),
    {
      id: 'root',
      items: [
        { name: 'match 1', nonsense: 'yes' },
        { name: 'match 2', nonsense: 'yes' },
        { name: 'match 4', nonsense: 'yes' },
      ],
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('find - by IS NOT type', async (t) => {
  const client = connect({ port: port }, { loglevel: 'info' })

  await client.set({
    $id: 'le1',
    type: 'league',
    name: 'league 1',
  })

  await client.set({
    $id: 'ma1',
    parents: ['le1'],
    type: 'match',
    name: 'match 1',
    value: 1,
  })

  const res = await client.get({
    matches: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '!=',
            $value: 'league',
          },
        },
      },
    },
  })
  t.is(res.matches.length, 1)

  const resWithLanguage = await client.get({
    $language: 'en',
    matches: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'type',
            $operator: '!=',
            $value: 'league',
          },
        },
      },
    },
  })
  t.is(resWithLanguage.matches.length, 1)

  await client.set({
    $id: 'ma2',
    parents: ['le1'],
    type: 'match',
    name: 'match 2',
    description: { en: 'some' },
    value: 1,
  })

  const resWithLanguage1 = await client.get({
    $language: 'en',
    matches: {
      id: true,
      $list: {
        $find: {
          $traverse: 'descendants',
          $filter: {
            $field: 'description',
            $operator: '!=',
            $value: 'some',
          },
        },
      },
    },
  })
  t.is(resWithLanguage1.matches.length, 2)

  await client.delete('root')
  await client.destroy()
})
