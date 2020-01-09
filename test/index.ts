import test from 'ava'
// expose types here (default), maybe expose things like id etc
import { connect, SelvaClient } from '../src/index'
import { start } from 'selva-server'

let db

const wait = () => new Promise(r => setTimeout(r, 500))

test.before(async t => {
  db = await start({ port: 6061, modules: ['redisearch'] })
  // This runs before all tests
})

test('generates a unique id', async t => {
  const client = connect({
    port: 6061
  })
  const id1 = client.id({ type: 'match' })
  const id2 = client.id({ type: 'match' })

  t.true(id1 !== id2)
  t.true(/ma.+/.test(id1))
  // new types what this means is that the client allways needs to load a map add it to prefix
  // allways subscribe on it (hash)
})

const logAll = async (client: SelvaClient) => {
  const ids = await client.redis.keys('*')
  console.log(
    (
      await Promise.all(
        ids.map(id =>
          id.indexOf('.') > -1
            ? client.redis.smembers(id)
            : client.redis.hgetall(id)
        )
      )
    ).map((v, i) => {
      return [ids[i], v]
    })
  )
}

test('set', async t => {
  const client = connect({
    port: 6061
  })

  // auto adds to root
  // if root does not exists it creates it
  const id = await client.set({
    type: 'match',
    title: { en: 'hello' }
  })

  const id2 = await client.set({
    type: 'league',
    title: { en: 'snurky' }
  })

  console.log(id, id2)

  const moreId = await client.set({
    type: 'person',
    parents: [id],
    title: { en: 'flurpy man' }
  })

  // move it
  await logAll(client)

  console.log('remove from previous parent')
  await client.set({
    $id: moreId,
    parents: [id2]
  })

  // should not need wait... strange
  // set needs to await for everything
  // await wait()
  await logAll(client)

  console.log('add extra previous parent')
  await client.set({
    $id: moreId,
    parents: {
      $add: id
    }
  })

  // remove ancestors
  // then children
  // then $add $delete syntax
  // await wait()
  await logAll(client)

  console.log('remove extra previous parent')
  await client.set({
    $id: moreId,
    parents: {
      $delete: id
    }
  })

  await logAll(client)

  console.log('add extra previous parent')
  await client.set({
    $id: moreId,
    parents: {
      $add: id
    }
  })

  await logAll(client)

  console.log('reset children', id, '[]')
  await client.set({
    $id: id,
    children: []
  })

  await logAll(client)

  console.log('reset children', id, moreId)
  await client.set({
    $id: id,
    children: [moreId]
  })

  await logAll(client)

  console.log('$add children', id2, id)
  await client.set({
    $id: id2,
    children: { $add: id }
  })

  await logAll(client)

  console.log('$delete children', id2, id)
  await client.set({
    $id: id2,
    children: { $delete: id }
  })

  await logAll(client)

  console.log('del person')
  await client.delete(moreId)
  await logAll(client)

  console.log('del league ')
  await client.delete(id2)
  await logAll(client)

  console.log('$add children', id, 'viDingDong')
  await client.set({
    $id: id,
    children: { $add: 'viDingDong' }
  })

  console.log('set default + increment')
  await client.set({
    $id: 'viDingDong',
    children: { $add: 'viDingDong2' },
    value: {
      $default: 100,
      $increment: 10
    }
  })
  await logAll(client)

  console.log('increment')
  await client.set({
    $id: 'viDingDong',
    children: { $add: 'viDingDong2' },
    value: {
      $default: 100,
      $increment: 10
    }
  })
  await logAll(client)

  // some cases
  // double parents deep

  console.log('del all')
  await client.delete({ $id: 'root' })
  await logAll(client)

  await wait()
})
