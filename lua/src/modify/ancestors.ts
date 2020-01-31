import { Id } from '../../../src/schema/index'
import * as redis from '../redis'
import { splitString, joinString } from '../util'

// order by depth (highest depth first)

// 2league,1root,1tag   //put

// zadd
// if 2 thing with same depth use sort

// includeAncestryWith
// excludeAncestryWith

// hierachy: false

// going trough all

// -- zSet keys

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

// change this .ancestor

// map {[id]}

// get an array of ids

//

const needAncestorUpdates: Record<Id, true> = {}
const depthMap: Record<Id, number> = {}

export function markForAncestorRecalculation(id: Id) {
  needAncestorUpdates[id] = true
}

// we need to treat depth as the min depth of all ancestors + 1
export function calculateDepth(id: Id): number {
  if (depthMap[id] !== undefined) {
    return depthMap[id]
  }

  const parents = redis.smembers(id + '.parents')
  for (const parent of parents) {
    if (depthMap[parent] !== undefined) {
      return 1 + depthMap[parent]
    }
  }
  // TODO
  return 0
}

export function reCalculateAncestors() {
  // Calculate the depth for every node that needs an ancestor update
  for (const id in needAncestorUpdates) {
    depthMap[id] = calculateDepth(id)
  }

  // TODO: sort ids by lowest depth first

  for (const id in needAncestorUpdates) {
    const parents = redis.smembers(id + '.parents')
    if (parents) {
      for (const parent of parents) {
        const parentAncestorKey = parent + '.ancestors'
        const parentAncestors: string[] = redis.zrangeWithScores(
          parentAncestorKey
        )
        // TODO: increment all depths in ancestors by one
        redis.zAddMultipleNew(parentAncestorKey, ...parentAncestors)
      }
    }
  }
}

// export function reCalculateAncestors(id: Id, parents?: Id[]) {
//   // batch.push(id)
//
//   if (!parents) {
//     parents = redis.smembers(id + '.parents')
//   }
//
//   let ancestors = getNewAncestors(parents)
//   redis.hset(id, 'ancestors', joinString(ancestors, ','))
//   const children = redis.smembers(id + '.children')
//   for (let child of children) {
//     reCalculateAncestors(child)
//   }
// }
