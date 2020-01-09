import { Id } from './schema'
import { SelvaClient } from './'

export async function getNewAncestors(
  client: SelvaClient,
  parents: Id[],
  from?: Id[]
): Promise<Set<string>> {
  const newAncestors: Set<string> = new Set(from)

  const ancestorsParents = await Promise.all(
    parents.map(k => client.redis.hget(k, 'ancestors'))
  )
  ancestorsParents.forEach((a: string | null) => {
    if (a) {
      const arr = a.split(',')
      if (arr.length) {
        arr.forEach(v => newAncestors.add(v))
      }
    }
  })
  parents.forEach(v => {
    newAncestors.add(v)
  })
  return newAncestors
}

export async function resetAncestors(
  client: SelvaClient,
  id: Id,
  parents: Id[],
  previousValue: Id[]
) {
  if (previousValue.length === 0) {
    const newAncestors = await getNewAncestors(client, parents)
    await client.redis.hset(id, 'ancestors', Array.from(newAncestors).join(','))
    const children = await client.redis.smembers(id + '.children')
    for (let child of children) {
      await addToAncestors(client, child, [id])
    }
  } else {
    console.log(
      '  🥰 ancestors changed! - means you need to remove the diff (intersection set)',
      id,
      parents
    )
    // need to remove shit as well
  }
}

export async function removeFromAncestors(
  client: SelvaClient,
  id: Id,
  parents: Id[]
) {
  console.log(' 🥰 Remove from ancestors!')
  // its about adding or remving parents
}

// start with this
export async function addToAncestors(
  client: SelvaClient,
  id: Id,
  parents: Id[]
) {
  // its about adding parents
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
