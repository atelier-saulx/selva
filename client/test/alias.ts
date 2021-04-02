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
                  dunk: { type: 'string' },
                },
              },
              dunk: {
                type: 'object',
                properties: {
                  ding: { type: 'number' },
                  dong: { type: 'number' },
                },
              },
            },
          },
          dong: { type: 'json' },
          dingdongs: { type: 'array', items: { type: 'string' } },
          refs: { type: 'references' },
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      custom: {
        prefix: 'cu',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      club: {
        prefix: 'cl',
        fields: {
          value: { type: 'number' },
          age: { type: 'number' },
          auth: {
            type: 'json',
          },
          title: { type: 'text' },
          description: { type: 'text' },
          image: {
            type: 'object',
            properties: {
              thumb: { type: 'string' },
              poster: { type: 'string' },
            },
          },
        },
      },
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          description: { type: 'text' },
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

test.serial('get non-existing by $alias', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'does_not_exists',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      $isNull: true,
    }
  )

  await client.delete('root')
  client.destroy()
})

test.serial('set alias and get by $alias', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    aliases: 'nice_match',
    type: 'match',
    title: { en: 'yesh' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      id: match1,
      title: 'yesh',
      aliases: ['nice_match'],
    }
  )

  t.deepEqualIgnoreOrder(await client.redis.hgetall('___selva_aliases'), {
    nice_match: match1,
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_object_get('', match1, 'aliases'),
    ['nice_match']
  )

  const match2 = await client.set({
    aliases: ['nice_match', 'very_nice_match'],
    type: 'match',
    title: { en: 'yesh2' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      id: match2,
      title: 'yesh2',
      aliases: ['nice_match', 'very_nice_match'],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'very_nice_match',
      id: true,
      title: true,
    }),
    {
      id: match2,
      title: 'yesh2',
    }
  )

  t.deepEqualIgnoreOrder(await client.redis.hgetall('___selva_aliases'), {
    nice_match: match2,
    very_nice_match: match2,
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_object_get('', match2, 'aliases'),
    ['nice_match', 'very_nice_match']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_object_get('', match1, 'aliases'),
    []
  )

  await client.set({
    $id: match1,
    aliases: { $add: ['ok_match'] },
  })

  await client.set({
    $id: match2,
    aliases: { $delete: ['very_nice_match'] },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      id: match2,
      title: 'yesh2',
      aliases: ['nice_match'],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'ok_match',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      id: match1,
      title: 'yesh',
      aliases: ['ok_match'],
    }
  )

  t.deepEqualIgnoreOrder(await client.redis.hgetall('___selva_aliases'), {
    ok_match: match1,
    nice_match: match2,
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_object_get('', match2, 'aliases'),
    ['nice_match']
  )

  t.deepEqualIgnoreOrder(
    await client.redis.selva_object_get('', match1, 'aliases'),
    ['ok_match']
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('set new entry with alias', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    $alias: 'nice_match',
    type: 'match',
    title: { en: 'yesh' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      id: match1,
      title: 'yesh',
      aliases: ['nice_match'],
    }
  )

  t.deepEqualIgnoreOrder(await client.redis.hgetall('___selva_aliases'), {
    nice_match: match1,
  })

  t.deepEqualIgnoreOrder(
    await client.redis.selva_object_get('', match1, 'aliases'),
    ['nice_match']
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('set existing entry with alias', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    $alias: 'nice_match',
    type: 'match',
    title: { en: 'yesh' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      id: match1,
      title: 'yesh',
      aliases: ['nice_match'],
    }
  )

  await client.set({
    $alias: ['not_so_nice_match', 'nice_match'], // second one exists
    type: 'match',
    title: { en: 'yesh yesh' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      id: match1,
      title: 'yesh yesh',
      aliases: ['nice_match'],
    }
  )

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: 'nice_match',
      id: true,
      title: true,
      aliases: true,
    }),
    {
      id: match1,
      title: 'yesh yesh',
      aliases: ['nice_match'],
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('set and get by $alias as id', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    type: 'match',
    title: { en: 'yesh' },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $alias: match1,
      id: true,
      title: true,
    }),
    {
      id: match1,
      title: 'yesh',
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('set parent by alias', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    type: 'match',
    title: { en: 'yesh' },
    aliases: {
      $add: 'match-1',
    },
  })

  const matchX = await client.set({
    type: 'match',
    title: { en: 'yeshX' },
  })

  const match2 = await client.set({
    type: 'match',
    title: { en: 'yesh-yesh' },
    parents: {
      $noRoot: true,
      $add: [
        {
          $alias: 'match-1',
          type: 'match',
        },
        {
          $id: matchX,
        },
        {
          $alias: 'non-existent',
          type: 'match',
        },
      ],
    },
  })

  const stub = await client.get({
    $alias: 'non-existent',
    id: true,
    parents: true,
  })

  t.deepEqualIgnoreOrder(stub.parents, [])

  t.deepEqualIgnoreOrder(
    await client.get({
      $language: 'en',
      $id: match2,
      title: true,
      parents: true,
    }),
    {
      title: 'yesh-yesh',
      parents: [match1, matchX, stub.id],
    }
  )

  await client.delete('root')
  await client.destroy()
})

test.serial('set parent by alias 2', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    type: 'match',
    title: { en: 'yesh' },
    aliases: ['snurk'],
  })

  const result = await client.get({
    $id: 'root',
    items: {
      id: true,
      $list: {
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

  console.dir(result, { depth: null })

  await client.set({
    type: 'custom',
    parents: { $add: [{ $id: match1 }] },
  })

  const result2 = await client.get({
    $id: 'root',
    items: {
      id: true,
      $list: {
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

  // console.dir(
  //   {
  //     result2,
  //     item: await client.get({
  //       $id: match1,
  //       $all: true,
  //       parents: true,
  //       ancestors: true,
  //     }),
  //   },
  //   { depth: null }
  // )

  t.deepEqualIgnoreOrder(result2.items, [{ id: match1 }])

  await client.delete('root')
  await client.destroy()
})

test.serial('delete all aliases of a node', async (t) => {
  const client = connect({ port }, { loglevel: 'info' })

  const match1 = await client.set({
    type: 'match',
    title: { en: 'yesh' },
    aliases: { $add: ['nice_match', 'nicer_match'] },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $alias: 'nice_match',
      id: true,
      aliases: true,
    }),
    {
      id: match1,
      aliases: ['nice_match', 'nicer_match'],
    }
  )
  t.deepEqualIgnoreOrder(
    await client.get({
      $alias: 'nicer_match',
      id: true,
      aliases: true,
    }),
    {
      id: match1,
      aliases: ['nice_match', 'nicer_match'],
    }
  )

  await client.set({
    $id: match1,
    aliases: { $delete: true },
  })

  t.deepEqualIgnoreOrder(
    await client.get({
      $id: match1,
      id: true,
      aliases: true,
    }),
    {
      id: match1,
    }
  )

  t.deepEqualIgnoreOrder(await client.redis.hgetall('___selva_aliases'), null)

  await client.delete('root')
  await client.destroy()
})
