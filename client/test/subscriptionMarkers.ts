import test from 'ava'
import { connect } from '../src/index'
import { start } from '@saulx/selva-server'
import redis, {RedisClient} from 'redis'
import './assertions'
import {wait} from './assertions'
import getPort from 'get-port'

let srv
let port: number
let rclient: RedisClient | null = null

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

  rclient = redis.createClient(port + 2)
})

test.after(async _t => {
  const client = connect({ port })
  await client.delete('root')
  await client.destroy()
  await srv.destroy()
})

test.afterEach(async () => {
  if (rclient) {
    rclient.end(true)
    rclient = null
  }
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

  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b61', '1', 'node', 'maTest0001')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b61')

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    ['2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b61']
  )

  const ds = await client.redis.selva_subscriptions_debug('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b61')
  t.is(ds.length, 1)
  const ds0 = ds[0]
  t.deepEqual(ds0[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b61')
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

  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b62', '1', 'node', 'maTest0001')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b62', '2', 'descendants', 'maTest0001')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b62')

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    ['2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b62']
  )

  const ds = await client.redis.selva_subscriptions_debug('___selva_hierarchy', '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b62')
  t.is(ds.length, 2, "subscription has two markers")
  const marker1 = ds[0]
  t.deepEqual(marker1[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b62')
  t.deepEqual(marker1[1], 'marker_id: 1')
  t.deepEqual(marker1[2], 'flags: 0x0002')
  t.deepEqual(marker1[3], 'node_id: "maTest0001"')
  t.deepEqual(marker1[4], 'dir: node')
  t.deepEqual(marker1[5], 'filter_expression: unset')
  const marker2 = ds[1]
  t.deepEqual(marker2[0], 'sub_id: 2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b62')
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
  const subId1 = '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b63';
  const subId2 = '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b64';

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

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'descendants', 'maTest0001')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId2, '1', 'ancestors', 'maTest0002')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId2)

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    [
      subId1,
      subId2
    ]
  )

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', subId1)
  t.is(s1.length, 1, "subscription1 has one marker")
  const s1marker1 = s1[0]
  t.deepEqual(s1marker1[0], `sub_id: ${subId1}`)
  t.deepEqual(s1marker1[1], 'marker_id: 1')
  t.deepEqual(s1marker1[2], 'flags: 0x0002')
  t.deepEqual(s1marker1[3], 'node_id: "maTest0001"')
  t.deepEqual(s1marker1[4], 'dir: bfs_descendants')
  t.deepEqual(s1marker1[5], 'filter_expression: unset')

  const s2 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', subId2)
  t.is(s2.length, 1, "subscription2 has one marker")
  const s2marker1 = s2[0]
  t.deepEqual(s2marker1[0], `sub_id: ${subId2}`)
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
  const subId1 = '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b65'
  const subId2 = '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b66'

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

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'descendants', 'maTest0001')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId2, '1', 'ancestors', 'maTest0002')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId2)
  await client.redis.selva_subscriptions_del('___selva_hierarchy', subId2)

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    [ subId1 ]
  )

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', subId1)
  t.is(s1.length, 1, "subscription1 has one marker")
  const s1marker1 = s1[0]
  t.deepEqual(s1marker1[0], `sub_id: ${subId1}`)
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
  const subId1 = '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b67'

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

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'ancestors', 'maTest0002')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)

  t.deepEqual(
    await client.redis.selva_subscriptions_list('___selva_hierarchy'),
    [ subId1 ]
  )

  await client.delete('maTest0002')

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', subId1)
  t.is(s1.length, 1, "subscription1 has one marker")
  const s1marker1 = s1[0]

  const dn1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0001')
  t.is(dn1.length, 0, "node has no more markers")

  await client.delete('root')
  client.destroy()
})

test.serial('Add nodes and verify propagation', async t => {
  const client = connect({ port })
  const subId1 = '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b68'

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

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'descendants', 'maTest0001')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)

  await client.set({
      $id: 'maTest0003',
      title: { en: 'ma3' },
      parents: [ 'maTest0001' ]
  })
  await client.set({
      $id: 'maTest0004',
      title: { en: 'ma4' },
      parents: [ 'maTest0002' ]
  })

  // The marker should propagate to the new nodes without refreshing the
  // subscription.
  const dn1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0001')
  t.is(dn1.length, 1, "node has one marker")
  const dn2 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0002')
  t.is(dn2.length, 1, "node has one marker")
  const dn3 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0003')
  t.is(dn3.length, 1, "node has one marker")
  const dn4 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', 'maTest0004')
  t.is(dn4.length, 1, "node has one marker")

  await client.delete('root')
  client.destroy()
})

test.serial('FindInSub: simple lookups', async t => {
  const client = connect({ port })
  const subId1 = 'fc35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69'

  await client.set({
    $id: 'maTest0001', // marker starts here
    title: { en: 'ma1' },
    children: [
        {
            $id: 'maTest0011', // child of the first level
            title: { en: 'ma11' },
            children: [
              {
                $id: 'maTest0021',
                title: { en: 'ma21' }
              }
            ],
            parents: [
              {
                $id: 'maTest0002', // Additional parent
                title: { en: 'ma02' }
              }
            ]
        },
        {
            $id: 'maTest0012', // child of the first level
            title: { en: 'ma12' },
        },
        {
            $id: 'maTest0013', // child of the first level
            title: { en: 'ma13' },
            children: [
              {
                $id: 'maTest0021',
                title: { en: 'ma21' },
                children: [
                  {
                    $id: 'maTest0031',
                    title: { en: 'ma31' }
                  }
                ]
              }
            ]
        }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'ancestors', 'maTest0031')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '2', 'descendants', 'maTest0001')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '3', 'children', 'maTest0001')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '4', 'parents', 'maTest0011')
  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '5', 'node', 'maTest0001')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', subId1)
  t.is(s1.length, 5, "subscription has all the markers")
  t.deepEqual(s1[0][1], 'marker_id: 1')
  t.deepEqual(s1[1][1], 'marker_id: 2')
  t.deepEqual(s1[2][1], 'marker_id: 3')
  t.deepEqual(s1[3][1], 'marker_id: 4')
  t.deepEqual(s1[4][1], 'marker_id: 5')

  t.deepEqual(
    await client.redis.selva_hierarchy_findinsub('___selva_hierarchy', subId1, 1),
    ['maTest0021', 'maTest0013', 'maTest0001', 'root']
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_findinsub('___selva_hierarchy', subId1, 2),
    [ 'maTest0011', 'maTest0012', 'maTest0013', 'maTest0021', 'maTest0031' ]
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_findinsub('___selva_hierarchy', subId1, 3),
    [ 'maTest0011', 'maTest0012', 'maTest0013' ]
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_findinsub('___selva_hierarchy', subId1, 4),
    [ 'maTest0001', 'maTest0002' ]
  )

  t.deepEqual(
    await client.redis.selva_hierarchy_findinsub('___selva_hierarchy', subId1, 5),
    [ 'maTest0001' ]
  )

  await client.delete('root')
  client.destroy()
})

test.serial('FindInSub: expression filter', async t => {
  const client = connect({ port })
  const subId1 = '1c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69'

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
        {
            $id: 'maTest0011',
            title: { en: 'test' },
            children: [
              {
                $id: 'maTest0021',
                title: { en: 'test' }
              }
            ]
        },
        {
            $id: 'maTest0012',
            title: { en: 'ma12' },
        },
        {
            $id: 'maTest0013',
            title: { en: 'ma13' },
            children: [
              {
                $id: 'maTest0022',
                title: { en: 'test' },
                children: [
                  {
                    $id: 'maTest0031',
                    title: { en: 'ma31' }
                  }
                ]
              }
            ]
        }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'descendants', 'maTest0001', '"title.en f $1 c', 'test')

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', subId1)
  t.is(s1.length, 1, "subscription1 has one marker")
  const s1marker1 = s1[0]
  t.deepEqual(s1marker1[0], `sub_id: ${subId1}`)
  t.deepEqual(s1marker1[1], 'marker_id: 1')
  t.deepEqual(s1marker1[2], 'flags: 0x0002')
  t.deepEqual(s1marker1[3], 'node_id: "maTest0001"')
  t.deepEqual(s1marker1[4], 'dir: bfs_descendants')
  t.deepEqual(s1marker1[5], 'filter_expression: set')
  t.deepEqual(s1marker1[6], 'fields: "(null)"')

  t.deepEqual(
    await client.redis.selva_hierarchy_findinsub('___selva_hierarchy', subId1, 1),
    ['maTest0011', 'maTest0021', 'maTest0022']
  )

  await client.delete('root')
  client.destroy()
})

test.serial('subscribe to hierarchy events', async t => {
  const subId1 = 'fc35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b70'
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'descendants', 'maTest0001', 'fields', 'descendants')

  let msgCount = 0
  const subChannel = `___selva_subscription_update:${subId1}`
  rclient.on('message', (channel, message) => {
    t.deepEqual(channel, subChannel)
    t.deepEqual(message, '')
    msgCount++
  })
  rclient.subscribe(`___selva_subscription_update:${subId1}`)

  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)
  await client.set({
    $id: 'maTest0002',
    title: { en: 'ma2' },
    parents: [
      'maTest0001',
    ]
  })
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)
  await client.set({
    $id: 'maTest0001',
    children: [
      {
        $id: 'maTest0003',
        title: { en: 'ma3' },
      }
    ]
  })

  await wait(100)
  t.assert(msgCount >= 2)

  await client.delete('root')
  client.destroy()
})

test.serial('FindInSub: expression filter and sort', async t => {
  const client = connect({ port })
  const subId1 = '2c35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b69'

  await client.set({
    $id: 'maTest0001',
    title: { en: 'z' },
    children: [
        {
            $id: 'maTest0011',
            title: { en: 'test' },
            children: [
              {
                $id: 'maTest0021',
                title: { en: 'test' }
              }
            ]
        },
        {
            $id: 'maTest0012',
            title: { en: 'o' },
        },
        {
            $id: 'maTest0013',
            title: { en: 'b' },
            children: [
              {
                $id: 'maTest0022',
                title: { en: 'x' },
                children: [
                  {
                    $id: 'maTest0031',
                    title: { en: 'a' }
                  }
                ]
              }
            ]
        }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'descendants', 'root', '"title.en f $1 c L', 'test')

  const s1 = await client.redis.selva_subscriptions_debug('___selva_hierarchy', subId1)
  t.is(s1.length, 1, "subscription1 has one marker")
  const s1marker1 = s1[0]
  t.deepEqual(s1marker1[0], `sub_id: ${subId1}`)
  t.deepEqual(s1marker1[1], 'marker_id: 1')
  t.deepEqual(s1marker1[2], 'flags: 0x0202')
  t.deepEqual(s1marker1[3], 'node_id: "root"')
  t.deepEqual(s1marker1[4], 'dir: bfs_descendants')
  t.deepEqual(s1marker1[5], 'filter_expression: set')
  t.deepEqual(s1marker1[6], 'fields: "(null)"')

  t.deepEqual(
    await client.redis.selva_hierarchy_findinsub('___selva_hierarchy', subId1, 1, 'order', 'title.en', 'asc', 'offset', '1', 'limit', 3),
    // b, o, x
    ['maTest0013', 'maTest0012', 'maTest0022']
  )

  await client.delete('root')
  client.destroy()
})

test.serial('subscribe to field events', async t => {
  const subId1 = 'fc35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b71'
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' },
    children: [
      {
        $id: 'maTest0002',
        title: { en: 'ma2' }
      },
      {
        $id: 'maTest0003',
        title: { en: 'ma3' }
      }
    ]
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'descendants', 'maTest0001', 'fields', 'title.en')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)

  let msgCount = 0
  const subChannel = `___selva_subscription_update:${subId1}`
  rclient.on('message', (channel, message) => {
    t.deepEqual(channel, subChannel)
    t.deepEqual(message, '')
    msgCount++
  })
  rclient.subscribe(`___selva_subscription_update:${subId1}`)

  await Promise.all([
    client.set({
      $id: 'maTest0001',
      title: { en: 'test1' },
    }),
    client.set({
      $id: 'maTest0002',
      title: { en: 'test2' },
    })
  ])

  await wait(100)
  t.assert(msgCount === 2)

  await client.delete('root')
  client.destroy()
})

test.serial('subscribe to field events with an expression', async t => {
  const subId1 = 'fc35a5a4782b114c01c1ed600475532641423b1bf5bf26a6645637e989f79b72'
  const client = connect({ port })

  await client.set({
    $id: 'maTest0001',
    title: { en: 'ma1' }
  })

  await client.redis.selva_subscriptions_add('___selva_hierarchy', subId1, '1', 'node', 'maTest0001', 'fields', 'title.en', '"title.en f $1 c', 'abc')
  await client.redis.selva_subscriptions_refresh('___selva_hierarchy', subId1)

  let msgCount = 0
  const subChannel = `___selva_subscription_update:${subId1}`
  rclient.on('message', (channel, message) => {
    t.deepEqual(channel, subChannel)
    t.deepEqual(message, '')
    msgCount++
  })
  rclient.subscribe(`___selva_subscription_update:${subId1}`)

  // expression match: 0 -> 1 => event
  client.set({
    $id: 'maTest0001',
    title: { en: 'abc' },
  })
  // expression match: 1 -> 1 => no event
  client.set({
    $id: 'maTest0001',
    title: { en: 'abc' },
  })
  // expression match: 1 -> 0 => event
  client.set({
    $id: 'maTest0001',
    title: { en: 'cba' },
  })
  // expression match: 0 -> 0 => no event
  client.set({
    $id: 'maTest0001',
    title: { en: 'xyz' },
  })
  // expression match: 0 -> 1 => event
  client.set({
    $id: 'maTest0001',
    title: { en: 'abc' },
  })

  await wait(100)
  t.assert(msgCount === 3)

  await client.delete('root')
  client.destroy()
})
