import test from 'ava'
import { connect } from '../src/index'
import { wait, worker } from './assertions'
import getPort from 'get-port'

test.after(async (t) => {
  await t.connectionsAreEmpty()
})

test.serial('subscribe and delete workerized', async (t) => {
  const port = await getPort()

  const [, registryWorker] = await worker(
    async ({ startRegistry }, { port }) => {
      await startRegistry({ port })
    },
    { port }
  )

  const [, originWorker] = await worker(
    async ({ startOrigin }, { port }) => {
      await startOrigin({ registry: { port }, name: 'default' })
    },
    { port }
  )

  const [, subscriptionWorker] = await worker(
    async ({ startSubscriptionManager }, { port }) => {
      await startSubscriptionManager({ registry: { port } })
    },
    { port }
  )

  const client = connect({ port })

  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    types: {
      thing: {
        prefix: 'th',
        fields: {
          yesh: { type: 'number' },
          next: { type: 'reference' },
          things: { type: 'references' },
        },
      },
    },
  })

  const q = []
  for (let i = 0; i < 10; i++) {
    q.push(
      client.set({
        type: 'thing',
        yesh: i,
      })
    )
  }

  const ids = await Promise.all(q)

  const observable = client.observe({
    $id: 'root',
    things: {
      id: true,
      yesh: true,
      $list: {
        $find: {
          $traverse: 'children', // also desc
          $filter: {
            $operator: '=',
            $value: 'thing',
            $field: 'type',
          },
        },
      },
    },
  })

  let cnt = 0

  const s = observable.subscribe((d) => {
    cnt++
  })

  await wait(1000)

  await client.delete({ $id: ids[0] })

  await wait(1000)

  t.is(cnt, 2)

  s.unsubscribe()
  registryWorker.terminate()
  subscriptionWorker.terminate()
  originWorker.terminate()

  await client.destroy()
})
