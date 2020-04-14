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

test.serial(
  'set without source, then set source, different source does not override',
  async t => {
    const client = connect({ port }, { loglevel: 'info' })

    const child = await client.set({
      type: 'match',
      title: { en: 'child' }
    })

    const match1 = await client.set({
      type: 'match',
      title: { en: 'yesh' },
      children: { $add: child }
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: match1,
        $language: 'en',
        id: true,
        title: true,
        children: true
      }),
      {
        id: match1,
        title: 'yesh',
        children: [child]
      }
    )

    await client.set({
      $id: match1,
      $source: 'yesh-source',
      type: 'match',
      title: { en: 'yesh2' },
      children: { $delete: child }
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: match1,
        $language: 'en',
        id: true,
        title: true
      }),
      {
        id: match1,
        title: 'yesh2'
      }
    )

    await client.set({
      $id: match1,
      $source: 'noes-source',
      type: 'match',
      title: { en: 'yesh3' },
      children: { $add: child }
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: match1,
        $language: 'en',
        id: true,
        title: true,
        children: true
      }),
      {
        id: match1,
        title: 'yesh2',
        children: []
      }
    )

    await client.set({
      $id: match1,
      $source: 'yesh-source',
      type: 'match',
      title: { en: 'yesh4' },
      children: { $add: child }
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: match1,
        $language: 'en',
        id: true,
        title: true,
        children: true
      }),
      {
        id: match1,
        title: 'yesh4',
        children: [child]
      }
    )

    await client.delete('root')
    client.destroy()
  }
)

test.serial(
  'set source, different source overrides only if matching $overwrite rule',
  async t => {
    const client = connect({ port }, { loglevel: 'info' })

    const child = await client.set({
      type: 'match',
      title: { en: 'child' }
    })

    const match1 = await client.set({
      type: 'match',
      title: { en: 'yesh' },
      children: { $add: child }
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $source: 'yesh-source',
        $id: match1,
        $language: 'en',
        id: true,
        title: true,
        children: true
      }),
      {
        id: match1,
        title: 'yesh',
        children: [child]
      }
    )

    await client.set({
      $id: match1,
      $source: { $overwrite: true, $name: 'yesh-source' },
      type: 'match',
      title: { en: 'yesh2' },
      children: { $delete: child }
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: match1,
        $language: 'en',
        id: true,
        title: true,
        children: true
      }),
      {
        id: match1,
        title: 'yesh2',
        children: []
      }
    )

    await client.set({
      $id: match1,
      $source: {
        $overwrite: ['noes-source', 'oh-noe-source'],
        $name: 'no-source'
      },
      type: 'match',
      title: { en: 'yesh3' },
      children: { $add: child }
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: match1,
        $language: 'en',
        id: true,
        title: true,
        children: true
      }),
      {
        id: match1,
        title: 'yesh2',
        children: []
      }
    )

    await client.set({
      $id: match1,
      $source: { $overwrite: ['yesh-source'], $name: 'yesh-box' },
      type: 'match',
      title: { en: 'yesh4' },
      children: { $add: child }
    })

    t.deepEqualIgnoreOrder(
      await client.get({
        $id: match1,
        $language: 'en',
        id: true,
        title: true,
        children: true
      }),
      {
        id: match1,
        title: 'yesh4',
        children: [child]
      }
    )

    // broken test for children/parents
    // await client.set({
    //   $id: child,
    //   $source: { $name: 'random-guy' },
    //   parents: { $delete: match1 }
    // })

    // t.deepEqualIgnoreOrder(
    //   await client.get({
    //     $id: match1,
    //     $language: 'en',
    //     id: true,
    //     title: true,
    //     children: true
    //   }),
    //   {
    //     id: match1,
    //     title: 'yesh4',
    //     children: [child]
    //   }
    // )

    await client.delete('root')
    client.destroy()
  }
)
