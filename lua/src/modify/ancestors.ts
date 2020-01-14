import { Id } from '../../../src/schema'
import * as redis from '../redis'
import { splitString, joinString } from '../util'

export function getNewAncestors(parents: Id[], from?: Id[]): string[] {
  let allAncestors: string[] = []
  let allAncestorsIdx = 0
  for (let i = 0; i < parents.length; i++) {
    const ancestors = redis.hget(parents[i], 'ancestors')
    if (ancestors) {
      const arr = splitString(ancestors, ',')
      if (arr.length > 0) {
        for (let i = 0; i < arr.length; i++) {
          allAncestors[allAncestorsIdx] = arr[i]
          allAncestorsIdx++
        }
      }
    }
  }

  return allAncestors
}

export function reCalculateAncestors(id: Id, parents?: Id[]) {
  if (!parents) {
    parents = redis.smembers(id + '.parents')
  }
  const ancestors = getNewAncestors(parents)
  redis.hset(id, 'ancestors', joinString(ancestors, ','))
  const children = redis.smembers(id + '.children')
  for (let child of children) {
    reCalculateAncestors(child)
  }
}
