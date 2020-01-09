import test from 'ava'
// expose types here (default), maybe expose things like id etc
import { connect, SelvaClient } from '../src/index'
import { start } from 'selva-server'

const wait = () => new Promise(r => setTimeout(r, 500))

test.before(async t => {
  await start({ port: 6061, modules: ['redisearch'] })
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

const dumpDb = async (client: SelvaClient): Promise<any[]> => {
  const ids = await client.redis.keys('*')
  return (
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
}

const logDb = async (client: SelvaClient) => {
  console.log(await dumpDb(client))
}

test('set', async t => {
  const client = connect({
    port: 6061
  })

  // simple setup
  const match = await client.set({
    type: 'match'
  })

  const league = await client.set({
    type: 'league'
  })

  const person = await client.set({
    type: 'person',
    parents: [match],
    title: { en: 'flurpy man' }
  })

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [person],
    'match has correct children'
  )

  t.deepEqual(
    (await client.redis.smembers('root.children')).sort(),
    [league, match].sort(),
    'root has correct children'
  )

  t.deepEqual(
    await client.redis.smembers(league + '.children'),
    [],
    'league has no children'
  )

  t.is(
    await client.redis.hget(person, 'title.en'),
    'flurpy man',
    'Title of person is correctly set'
  )

  t.is(
    await client.redis.hget(match, 'ancestors'),
    'root',
    'match has correct ancestors'
  )

  t.is(
    await client.redis.hget(league, 'ancestors'),
    'root',
    'league has correct ancestors'
  )

  // move person from match to league
  await client.set({
    $id: person,
    parents: [league]
  })

  t.deepEqual(
    await client.redis.smembers(league + '.children'),
    [person],
    'league has person after move'
  )

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [],
    'match has no children after move'
  )

  t.is(
    await client.redis.hget(person, 'ancestors'),
    ['root', league].join(','),
    'person has correct ancestors after move'
  )

  // add extra parent using $add
  await client.set({
    $id: person,
    parents: {
      $add: match
    }
  })

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [person],
    'match has children after $add'
  )

  t.deepEqual(
    (await client.redis.smembers(person + '.parents')).sort(),
    [league, match].sort(),
    'person has correct parents after $add'
  )

  t.is(
    await client.redis.hget(person, 'ancestors'),
    ['root', league, match].join(','),
    'person has correct ancestors after $add'
  )

  // remove league from person
  await client.set({
    $id: person,
    parents: {
      $delete: league
    }
  })

  t.deepEqual(
    await client.redis.smembers(league + '.children'),
    [],
    'league has no children after $delete'
  )

  t.deepEqual(
    await client.redis.smembers(person + '.parents'),
    [match],
    'person has correct parents after $delete'
  )

  t.is(
    await client.redis.hget(person, 'ancestors'),
    ['root', match].join(','),
    'person has correct ancestors after $delete'
  )

  // add parent again
  await client.set({
    $id: person,
    parents: {
      $add: league
    }
  })

  // double add
  await client.set({
    $id: person,
    parents: {
      $add: league
    }
  })

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [person],
    'match has children after 2nd $add'
  )

  t.deepEqual(
    (await client.redis.smembers(person + '.parents')).sort(),
    [league, match].sort(),
    'person has correct parents after 2nd $add'
  )

  t.is(
    await client.redis.hget(person, 'ancestors'),
    ['root', match, league].join(','),
    'person has correct ancestors after 2nd $add'
  )

  // reset children
  await client.set({
    $id: match,
    children: []
  })

  t.deepEqual(
    await client.redis.smembers(match + '.children'),
    [],
    'match has no children after reset'
  )

  t.deepEqual(
    (await client.redis.smembers(person + '.parents')).sort(),
    [league].sort(),
    'person has correct parents after reset of children of match'
  )

  t.is(
    await client.redis.hget(person, 'ancestors'),
    ['root', league].join(','),
    'person has correct ancestors after reset of children of match'
  )

  await logDb(client)

  // await logAll(client)

  // console.log('reset children', id, moreId)
  // await client.set({
  //   $id: id,
  //   children: [moreId]
  // })

  // await logAll(client)

  // console.log('$add children', id2, id)
  // await client.set({
  //   $id: id2,
  //   children: { $add: id }
  // })

  // await logAll(client)

  // console.log('$delete children', id2, id)
  // await client.set({
  //   $id: id2,
  //   children: { $delete: id }
  // })

  // await logAll(client)

  // console.log('del person')
  // await client.delete(moreId)
  // await logAll(client)

  // console.log('del league ')
  // await client.delete(id2)
  // await logAll(client)

  // console.log('$add children', id, 'viDingDong')
  // await client.set({
  //   $id: id,
  //   children: { $add: 'viDingDong' }
  // })

  // console.log('set default + increment')
  // await client.set({
  //   $id: 'viDingDong',
  //   children: { $add: 'viDingDong2' },
  //   value: {
  //     $default: 100,
  //     $increment: 10
  //   }
  // })
  // await logAll(client)

  // console.log('increment')
  // await client.set({
  //   $id: 'viDingDong',
  //   children: { $add: 'viDingDong2' },
  //   value: {
  //     $default: 100,
  //     $increment: 10
  //   }
  // })
  // await logAll(client)

  // // some cases
  // // double parents deep - important
  // console.log('$add children', 'maSmurkels + viDingDong', 'viDingDong3')
  // await client.set({
  //   $id: 'maSmurkels',
  //   children: { $add: 'viDingDong3' }
  // })
  // await client.set({
  //   $id: 'viDingDong',
  //   children: { $add: 'viDingDong3' }
  // })
  // await logAll(client)

  // console.log('del all')
  // await client.delete({ $id: 'root' })
  // await logAll(client)

  // console.log('do it again')
  // await client.set({
  //   $id: 'maSmurkels',
  //   children: { $add: 'viDing' }
  // })
  // await client.set({
  //   $id: 'viDing',
  //   children: { $add: 'viDong' }
  // })
  // await logAll(client)
})
