import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import './assertions'
import getPort from 'get-port'

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
  const client = connect({ port }, { loglevel: 'info' })

  await client.redis.flushall()
  await client.updateSchema({
    languages: ['en', 'de', 'nl'],
    rootType: {
      fields: {
        value: { type: 'number' },
        nested: {
          type: 'object',
          properties: {
            fun: { type: 'string' }
          }
        }
      }
    },
    types: {
      match: {
        prefix: 'ma',
        fields: {
          title: { type: 'text' },
          value: { type: 'number' },
          description: { type: 'text' }
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

test.serial('create a node marker', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
        {
            $id: 'maTest0002',
            title: { en: 'ma2' },
        }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68', '1', 'node', 'maTest0001')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    ['2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68']
  )

  const ds = await client.redis.selva_subscriptions_debug('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.is(ds.length, 1)
  const ds0 = ds[0]
  t.deepEqual(ds0[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.deepEqual(ds0[1], 'marker_id: 1')
  t.deepEqual(ds0[2], 'flags: 0x0002')
  t.deepEqual(ds0[3], 'node_id: "maTest0001"')
  t.deepEqual(ds0[4], 'dir: node')
  t.deepEqual(ds0[5], 'filter_expression: unset')

  const dn1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0001')
  t.is(dn1.length, 1)
  t.deepEqual(ds0[0], dn1[0][0])
  t.deepEqual(ds0[1], dn1[0][1])

  const dn2 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0002')
  t.is(dn2.length, 0)

  await client.delete('root')
  client.destroy()
})

test.serial('create two node markers', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
        {
            $id: 'maTest0002',
            title: { en: 'ma2' },
        }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68', '1', 'node', 'maTest0001')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68', '2', 'descendants', 'maTest0001')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    ['2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68']
  )

  const ds = await client.redis.selva_subscriptions_debug('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.is(ds.length, 2, "subscription has two markers")
  const marker1 = ds[0]
  t.deepEqual(marker1[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.deepEqual(marker1[1], 'marker_id: 1')
  t.deepEqual(marker1[2], 'flags: 0x0002')
  t.deepEqual(marker1[3], 'node_id: "maTest0001"')
  t.deepEqual(marker1[4], 'dir: node')
  t.deepEqual(marker1[5], 'filter_expression: unset')
  const marker2 = ds[1]
  t.deepEqual(marker2[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.deepEqual(marker2[1], 'marker_id: 2')
  t.deepEqual(marker2[2], 'flags: 0x0002')
  t.deepEqual(marker2[3], 'node_id: "maTest0001"')
  t.deepEqual(marker2[4], 'dir: bfs_descendants')
  t.deepEqual(marker2[5], 'filter_expression: unset')

  // The first node has both markers
  const dn1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0001')
  t.is(dn1.length, 2, "node has two markers")
  t.deepEqual(marker1[0], dn1[0][0], "maker1 sub_id matches")
  t.deepEqual(marker1[1], dn1[0][1])
  t.deepEqual(marker2[0], dn1[1][0], "marker2 sub_id matches")
  t.deepEqual(marker2[1], dn1[1][1])

  // The second node has only the traversing marker
  const dn2 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0002')
  t.is(dn2.length, 1, "node has one marker")
  t.deepEqual(marker2[0], dn2[0][0])
  t.deepEqual(marker2[1], dn2[0][1])

  await client.delete('root')
  client.destroy()
})

test.serial('create two subscriptions', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
        {
            $id: 'maTest0002',
            title: { en: 'ma2' },
        }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68', '1', 'descendants', 'maTest0001')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69', '1', 'ancestors', 'maTest0002')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69')

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    [
      '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68',
      '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69'
    ]
  )

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.is(s1.length, 1, "subscription1 has one marker")
  const s1marker1 = s1[0]
  t.deepEqual(s1marker1[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.deepEqual(s1marker1[1], 'marker_id: 1')
  t.deepEqual(s1marker1[2], 'flags: 0x0002')
  t.deepEqual(s1marker1[3], 'node_id: "maTest0001"')
  t.deepEqual(s1marker1[4], 'dir: bfs_descendants')
  t.deepEqual(s1marker1[5], 'filter_expression: unset')

  const s2 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69')
  t.is(s2.length, 1, "subscription2 has one marker")
  const s2marker1 = s2[0]
  t.deepEqual(s2marker1[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69')
  t.deepEqual(s2marker1[1], 'marker_id: 1')
  t.deepEqual(s2marker1[2], 'flags: 0x0002')
  t.deepEqual(s2marker1[3], 'node_id: "maTest0002"')
  t.deepEqual(s2marker1[4], 'dir: bfs_ancestors')
  t.deepEqual(s2marker1[5], 'filter_expression: unset')

  const dn1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0001')
  t.is(dn1.length, 2, "node has two markers")
  t.deepEqual(s1marker1[0], dn1[0][0])
  t.deepEqual(s1marker1[1], dn1[0][1])
  t.deepEqual(s2marker1[0], dn1[1][0])
  t.deepEqual(s2marker1[1], dn1[1][1])

  const dn2 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0002')
  t.is(dn2.length, 2, "node has two markers")
  t.deepEqual(s1marker1[0], dn2[0][0])
  t.deepEqual(s1marker1[1], dn2[0][1])
  t.deepEqual(s2marker1[0], dn2[1][0])
  t.deepEqual(s2marker1[1], dn2[1][1])

  await client.delete('root')
  client.destroy()
})

test.serial('Delete a subscription', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
        {
            $id: 'maTest0002',
            title: { en: 'ma2' },
        }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68', '1', 'descendants', 'maTest0001')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69', '1', 'ancestors', 'maTest0002')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69')
  await client.redis.selva_subscriptions_del('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69')

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    [
      '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68'
    ]
  )

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.is(s1.length, 1, "subscription1 has one marker")
  const s1marker1 = s1[0]
  t.deepEqual(s1marker1[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68')
  t.deepEqual(s1marker1[1], 'marker_id: 1')
  t.deepEqual(s1marker1[2], 'flags: 0x0002')
  t.deepEqual(s1marker1[3], 'node_id: "maTest0001"')
  t.deepEqual(s1marker1[4], 'dir: bfs_descendants')
  t.deepEqual(s1marker1[5], 'filter_expression: unset')

  const dn1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0001')
  t.is(dn1.length, 1, "node has two markers")
  t.deepEqual(s1marker1[0], dn1[0][0])
  t.deepEqual(s1marker1[1], dn1[0][1])

  const dn2 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0002')
  t.is(dn2.length, 1, "node has two markers")
  t.deepEqual(s1marker1[0], dn2[0][0])
  t.deepEqual(s1marker1[1], dn2[0][1])

  await client.delete('root')
  client.destroy()
})

test.serial('Delete a node', async t => {
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
        {
            $id: 'maTest0002',
            title: { en: 'ma2' },
        }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69', '1', 'ancestors', 'maTest0002')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69')

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    [
      '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69'
    ]
  )

  await client.delete('maTest0002')

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69')
  t.is(s1.length, 1, "subscription1 has one marker")
  const s1marker1 = s1[0]

  const dn1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0001')
  t.is(dn1.length, 0, "node has more markers")

  await client.delete('root')
  client.destroy()
})
