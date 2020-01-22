import { Id } from '~selva/schema/index'
import { reCalculateAncestors } from './ancestors'
import * as redis from '../redis'

export function deleteItem(id: Id, hierarchy: boolean = true): boolean {
  if (hierarchy) {
    const children = redis.smembers(id + '.children')
    const parents = redis.smembers(id + '.parents')
    for (let parent of parents) {
      redis.srem(parent + '.children', id)
    }
    for (let child of children) {
      const key = child + '.parents'
      redis.srem(key, id)
      const size = redis.scard(key)
      if (size === 0) {
        deleteItem(child)
      } else {
        reCalculateAncestors(id, parents)
      }
    }
  }
  redis.del(id + '.children')
  redis.del(id + '.parents')
  // returns true if it existed
  return redis.del(id) > 0
}
