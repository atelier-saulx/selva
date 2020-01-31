import { Id } from '../../../src/schema/index'
import * as redis from '../redis'
import { splitString, joinString } from '../util'
import { addFieldToSearch } from './search'

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

const batch = {}
const executed = {}

export function reCalculateAncestors(id: Id, parents?: Id[]) {
  // batch.push(id)

  if (!parents) {
    parents = redis.smembers(id + '.parents')
  }

  let ancestors = getNewAncestors(parents)
  const stringAncestors = joinString(ancestors, ',')
  redis.hset(id, 'ancestors', stringAncestors)
  addFieldToSearch(id, 'ancestors', stringAncestors)
  const children = redis.smembers(id + '.children')
  for (let child of children) {
    reCalculateAncestors(child)
  }
}

export function exec() {
  // batch
}
