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

export function getDepth(id: Id): number {
  if (depthMap[id]) {
    return depthMap[id]
  }

  const depth = tonumber(redis.get(id + '._depth'))
  if (!depth) {
    return 0
  }

  return depth
}

export function setDepth(id: Id, depth: number): void {
  if (depthMap[id] === depth) {
    // cache not changed, bail
    return
  }

  depthMap[id] = depth
  redis.set(id + '._depth', tostring(id))
}

// we need to treat depth as the min depth of all ancestors + 1
export function updateDepths(id: Id): void {
  const currentDepth = getDepth(id)
  if (currentDepth) {
    // when updating an existing id, we don't need to make depth updates
    return
  }

  // update self depth
  const parents = redis.smembers(id + '.parents')
  let minParentDepth: number
  for (const parent of parents) {
    const parentDepth = getDepth(parent)
    if (parentDepth && (!minParentDepth || minParentDepth > parentDepth)) {
      minParentDepth = parentDepth
    }
  }

  setDepth(id, 1 + minParentDepth)

  // update depth of all children
  const children = redis.smembers(id + '.children')
  if (!children) {
    return
  }

  for (const child of children) {
    updateDepths(child)
    // update the depth of self in child ancestors
    redis.zAddReplaceScore(child + '.ancestors', 1 + minParentDepth, id)
  }
}

export function reCalculateAncestors() {
  const ids: Id[] = []
  for (const id in needAncestorUpdates) {
    updateDepths(id)
    ids[ids.length] = id
  }

  // we want to update ancestors frow lowest to deepest
  table.sort(ids, (a, b) => {
    return depthMap[a] <= depthMap[b]
  })

  for (const id in needAncestorUpdates) {
    const parents = redis.smembers(id + '.parents')
    if (parents) {
      for (const parent of parents) {
        const parentAncestorKey = parent + '.ancestors'
        const parentAncestors: string[] = redis.zrangeWithScores(
          parentAncestorKey
        )

        for (let i = 0; i < parentAncestors.length; i += 2) {
          const ancestorDepth = parentAncestors[i + 1]
          parentAncestors[i + 1] = tostring(1 + tonumber(ancestorDepth))
        }

        // clear the ancestors in case of any removed ancestors
        redis.del(parentAncestorKey)
        // set new ancestors in place from parents
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
