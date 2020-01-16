import { Id } from '../../../src/schema'
import * as redis from '../redis'
import { splitString, joinString } from '../util'

export function getNewAncestors(parents: Id[], from?: Id[]): string[] {
  let allAncestors: { [k: string]: boolean } = {}
  for (let i = 0; i < parents.length; i++) {
    let ancestors = redis.hget(parents[i], 'ancestors')
    if (ancestors && ancestors.length > 0) {
      const arr = splitString(ancestors, ',')
      if (arr.length > 0) {
        for (let j = 0; j < arr.length; j++) {
          const ancestor = arr[j]

          if (ancestor && ancestor.length > 0) {
            allAncestors[ancestor] = true
          }
        }
      }
    }
  }

  for (let i = 0; i < parents.length; i++) {
    allAncestors[parents[i]] = true
  }

  const result: string[] = []
  let i = 0
  for (const ancestor in allAncestors) {
    result[i] = ancestor
    i++
  }

  if (result.length === 0) {
    result[0] = 'root'
  }

  return result
}

// change this
export function reCalculateAncestors(id: Id, parents?: Id[]) {
  if (!parents) {
    parents = redis.smembers(id + '.parents')
  }

  let ancestors = getNewAncestors(parents)
  redis.hset(id, 'ancestors', joinString(ancestors, ','))
  const children = redis.smembers(id + '.children')
  for (let child of children) {
    reCalculateAncestors(child)
  }
}
