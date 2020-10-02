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
      folder: {
        prefix: 'fo',
        fields: { title: { type: 'text' } }
      },
      match: {
        prefix: 'ma',
        fields: {
          published: { type: 'boolean', search: true },
          buttonText: { type: 'text', search: true }
        }
      }
    }
  })

  await client.set({
    $language: 'en',
    $id: 'fo1'
  })

  const obs = client.observe(
    {
      $id: 'fo1',
      $all: true,
      $language: 'en',
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
    if (v.children[0]) {
      results.push(v.children[0].buttonText)
    }
  })

  client.set({
    $id: 'fo1',
    $language: 'en',
    children: [
      {
        $id: 'ma1',
        buttonText: 'my ballz'
      }
    ]
  })

  await wait(100)

  client.set({
    $id: 'ma1',
    $language: 'en',
    buttonText: 'my ball'
  })

  await wait(100)

  client.set({
    $id: 'ma1',
    $language: 'en',
    buttonText: 'my ba'
  })

  await wait(100)

  client.set({
    $id: 'ma1',
    $language: 'en',
    buttonText: 'my bal'
  })

  await wait(100)

  client.set({
    $id: 'ma1',
    $language: 'en',
    buttonText: 'my ballz'
  })

  await wait(100)

  client.set({
    $id: 'ma1',
    $language: 'en',
    buttonText: 'my ballzzzz'
  })

  await wait(100)

  console.log(results)

  await client.destroy()
  await Promise.all(servers.map(s => s.destroy()))
  await t.connectionsAreEmpty()
})
