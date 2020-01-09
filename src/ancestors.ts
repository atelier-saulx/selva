import { Id } from './schema'
import { SelvaClient } from './'

export async function getNewAncestors(
  client: SelvaClient,
  parents: Id[],
  from?: Id[]
): Promise<Set<string>> {
  const ancestors: Set<string> = new Set(from)
  const ancestorsParents = await Promise.all(
    parents.map(k => client.redis.hget(k, 'ancestors'))
  )
  ancestorsParents.forEach((a: string | null) => {
    if (a) {
      const arr = a.split(',')
      if (arr.length) {
        arr.forEach(v => ancestors.add(v))
      }
    }
  })
  parents.forEach(v => {
    ancestors.add(v)
  })
  return ancestors
}

export async function resetAncestors(
  client: SelvaClient,
  id: Id,
  parents: Id[],
  previousParents: Id[]
) {
  const ancestors = await getNewAncestors(client, parents)
  if (previousParents.length !== 0) {
    const previousAncestors = await getNewAncestors(client, previousParents)
    const ancestors = await getNewAncestors(client, parents)
    const toRemove = [...previousAncestors].filter(k => !ancestors.has(k))
    if (toRemove.length) {
      await removeFromAncestors(client, id, toRemove)
    }
  }
  await client.redis.hset(id, 'ancestors', Array.from(ancestors).join(','))
  const children = await client.redis.smembers(id + '.children')
  for (let child of children) {
    await addToAncestors(client, child, [id])
  }
}

export async function removeFromAncestors(
  client: SelvaClient,
  id: Id,
  parents: Id[]
) {
  const ancestors = await client.redis.hget(id, 'ancestors')
  const newAncestors = ancestors
    ? await getNewAncestors(client, [], ancestors.split(','))
    : await getNewAncestors(client, [])

  console.log('SET', newAncestors)

  console.log(' 🥰 Remove from ancestors!', id, parents)
}

export async function addToAncestors(
  client: SelvaClient,
  id: Id,
  parents: Id[]
) {
  if (!(await client.redis.sismember(id + '.parents', id))) {
    const ancestors = await client.redis.hget(id, 'ancestors')
    const newAncestors = ancestors
      ? await getNewAncestors(client, parents, ancestors.split(','))
      : await getNewAncestors(client, parents)
    await client.redis.hset(id, 'ancestors', Array.from(newAncestors).join(','))
    const children = await client.redis.smembers(id + '.children')
    for (let child of children) {
      await addToAncestors(client, child, [id])
    }
  } else {
    console.warn('  🥰 Parent allready exists!', id)
  }
}
