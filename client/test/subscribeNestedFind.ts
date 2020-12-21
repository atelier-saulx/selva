import test from 'ava'
import { connect } from '../src/index'
import {
  startSubscriptionManager,
  startSubscriptionRegistry,
  startReplica,
  startOrigin,
  startRegistry
} from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'
import { wait, worker, removeDump } from './assertions'
import { join } from 'path'
const dir = join(process.cwd(), 'tmp', 'subscribe-nested-find-test')

test.before(removeDump(dir))
test.after(removeDump(dir))

test.serial('get - correct order', async t => {
  const port = await getPort()
  const servers = await Promise.all([
    startRegistry({ port }),
    startOrigin({
      dir,
      registry: { port },
      default: true
    }),
    startSubscriptionManager({ registry: { port } }),
    startSubscriptionRegistry({ registry: { port } }),
    startReplica({
      dir: join(dir, 'replica'),
      registry: { port },
      default: true
    })
  ])

  const client = connect({ port }, { loglevel: 'info' })
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      folder: {
        prefix: 'fl',
        fields: {
          published: { type: 'boolean', search: true },
          title: { type: 'text', search: true }
        }
      },
      region: {
        prefix: 're',
        fields: {
          published: { type: 'boolean', search: true },
          title: { type: 'text', search: true }
        }
      },
      match: {
        prefix: 'ma',
        fields: {
          published: { type: 'boolean', search: true },
          title: { type: 'text', search: true }
        }
      }
    }
  })

  await client.set({
    $id: 'root',
    $language: 'en',
    children: [
      {
        type: 'folder',
        title: 'stuff',
        children: [
          {
            $id: 'ma1',
            title: 'match 1',
            published: true
          },
          {
            $id: 'ma2',
            title: 'match 2',
            published: true
          },
          {
            $id: 'ma3',
            title: 'match 3',
            published: true
          },
          {
            $id: 'ma4',
            title: 'match 4',
            published: false
          }
        ]
      },
      {
        type: 'region',
        $id: 're1',
        title: 'duitsland',
        published: true,
        children: [
          {
            type: 'folder',
            name: 'Highlights',
            published: true,
            children: ['ma1', 'ma2', 'ma3', 'ma4']
          }
        ]
      }
    ]
  })

  const obs = {
    $id: 're1',
    children: {
      title: true,
      published: true,
      $list: {
        $find: {
          $traverse: 'children',
          $filter: [
            {
              $field: 'type',
              $operator: '=',
              $value: 'folder'
            },
            {
              $field: 'name',
              $operator: '=',
              $value: 'Highlights'
            }
          ],
          $find: {
            $traverse: 'children',
            $filter: [
              {
                $field: 'published',
                $operator: '=',
                $value: true
              }
            ]
          }
        },
        $limit: 8
      }
    }
  }

  // const x = await client.get({ ...obs, $includeMeta: true })

  // console.dir(x.$meta, { depth: 10 })

  const results = []

  client.observe(obs, { immutable: true }).subscribe(v => {
    results.push(v)
  })

  await wait(1e3)

  client.set({ $id: 'ma1', published: false })

  await wait(1e3)

  client.set({ $id: 'ma1', published: true })

  await wait(3e3)

  t.is(results.length, 3)
  t.is(results[0].children.length, 3)
  t.is(results[1].children.length, 2)
  t.is(results[2].children.length, 3)

  await client.destroy()
  await Promise.all(servers.map(s => s.destroy()))
  await t.connectionsAreEmpty()

  t.pass()
})
