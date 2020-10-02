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

test.serial('no json parsing', async t => {
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
      match: {
        prefix: 'ma',
        fields: {
          published: { type: 'boolean', search: true },
          title: { type: 'text', search: true }
        }
      }
    }
  })

  const obs = client.observe(
    {
      children: {
        $list: true,
        $all: true
      }
    },
    {
      immutable: true
    }
  )

  const results = []

  obs.subscribe((v, hash, diff) => {
    console.log(v, hash, diff)

    results.push(v)
  })

  client.set({
    $id: 'root',
    $language: 'en',
    children: [
      {
        $id: 'ma1',
        title: 'my ballz',
        published: true
      }
    ]
  })

  await wait(100)

  client.set({
    $id: 'ma1',
    $language: 'en',
    title: 'my ball',
    published: true
  })

  await wait(100)

  client.set({
    $id: 'ma1',
    $language: 'en',
    title: 'my ba',
    published: true
  })

  await wait(100)

  await client.destroy()
  await Promise.all(servers.map(s => s.destroy()))
  await t.connectionsAreEmpty()
})
