import { Id, Language, Type, getTypeFromId } from './schema'
import { SelvaClient } from '.'

// create ancestors according to schema
// if ignore etc

type Ancestor = [Ancestor[], number]

// only shortest ancestor

// memoize this in lua (within one batch of gets)
// const ancestorMap = {} etc

// can prob just use the ancestors field (store )

// dependeing on the schema for inherit this will do stuff

const createAncestorsInner = async (
  client: SelvaClient,
  id: Id,
  s: Record<Id, Ancestor>
): Promise<Ancestor> => {
  // if memoized[id] -> get it
  if (s[id]) {
    return s[id]
  }
  const parents = await client.redis.smembers(id + '.parents')
  const ancestor: Ancestor = [[], 0]
  if (parents.length) {
    ancestor[1] = 1
    let pd = 0
    for (let pId of parents) {
      const a = await createAncestorsInner(client, pId, s)
      if (a[1] > pd) {
        pd = a[1]
        a[0].unshift(a)
      } else {
        a[0].push(a)
      }
    }
    ancestor[1] += pd
  }
  s[id] = ancestor
  return ancestor
}

export default createAncestorsInner
