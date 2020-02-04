import { Id, Schema } from '../../../src/schema/index'
import * as redis from '../redis'
import * as logger from '../logger'
import { splitString, joinString } from '../util'
import { addFieldToSearch } from './search'
import { getSchema } from '../schema/index'
import { getTypeFromId } from '../typeIdMapping'

// order by depth (highest depth first)

// 2league,1root,1tag   //put

// zadd
// if 2 thing with same depth use sort

// includeAncestryWith
// excludeAncestryWith

// hierachy: false

// going trough all

// -- zSet keys

// change this .ancestor

// map {[id]}

// get an array of ids

//

const schema: Schema = getSchema()
const needAncestorUpdates: Record<Id, true> = {}
const alreadyUpdated: Record<Id, true> = {}
const depthMap: Record<Id, number> = {}

type IncludeAncestryRule = { includeAncestryWith: string[] }
type ExcludeAncestryWith = { excludeAncestryWith: string[] }
type AncestryRule = IncludeAncestryRule | ExcludeAncestryWith | false

function isIncludeAncestryRule(
  rule: AncestryRule | null
): rule is IncludeAncestryRule {
  if (!rule) {
    return false
  }

  return !!(<any>rule).includeAncestryWith
}

function isExcludeAncestryRule(
  rule: AncestryRule | null
): rule is IncludeAncestryRule {
  if (!rule) {
    return false
  }

  return !!(<any>rule).includeAncestryWith
}

function includeAncestors(
  includedTypes: string[],
  ancestors: string[]
): string[] {
  for (let i = 0; i < ancestors.length; i++) {
    for (const includedTypeName of includedTypes) {
      if (getTypeFromId(ancestors[i]) === includedTypeName) {
        return ancestors
      }
    }
  }

  return []
}

function excludeAncestors(
  excludedTypes: string[],
  ancestors: string[]
): string[] {
  for (let i = 0; i < ancestors.length; i++) {
    for (const excludedTypeName of excludedTypes) {
      if (getTypeFromId(ancestors[i]) === excludedTypeName) {
        return []
      }
    }
  }

  return ancestors
}

function ancestryFromHierarchy(id: Id, parent: Id): string[] {
  const ancestors = redis.zrangeWithScores(parent + '.ancestors')

  // can't map, should never happen though
  if (!schema.prefixToTypeMapping) {
    return ancestors
  }

  const typeName: string = schema.prefixToTypeMapping[id.substring(0, 2)]
  const hierarchy = schema.types[typeName].hierarchy

  if (!hierarchy) {
    return ancestors
  }

  let foundRule: AncestryRule | null = null

  for (const ruleTypeName in hierarchy) {
    if (ruleTypeName === typeName) {
      foundRule = hierarchy[ruleTypeName]
      break
    }
  }

  if (foundRule === false) {
    return []
  } else if (isIncludeAncestryRule(foundRule)) {
    return includeAncestors(foundRule.includeAncestryWith, ancestors)
  } else if (isExcludeAncestryRule(foundRule)) {
    excludeAncestors(foundRule.excludeAncestryWith, ancestors)
  } else if (hierarchy.$default) {
    const rule = hierarchy.$default
    if (isIncludeAncestryRule(rule)) {
      return includeAncestors(rule.includeAncestryWith, ancestors)
    }

    return excludeAncestors(rule.excludeAncestryWith, ancestors)
  }

  return ancestors
}

export function markForAncestorRecalculation(id: Id) {
  needAncestorUpdates[id] = true
}

function getDepth(id: Id): number | false {
  if (id === 'root') {
    return 0
  }

  if (depthMap[id]) {
    return depthMap[id]
  }

  const depth = tonumber(redis.get(id + '._depth'))
  if (!depth) {
    return false
  }

  return depth
}

function setDepth(id: Id, depth: number): boolean {
  if (depthMap[id] === depth) {
    // cache not changed, bail
    return false
  }

  depthMap[id] = depth
  redis.set(id + '._depth', tostring(depth))
  return true
}

// we need to treat depth as the min depth of all ancestors + 1
function updateDepths(id: Id): void {
  // update self depth
  const parents = redis.smembers(id + '.parents')
  let maxParentDepth: number | null = null
  for (const parent of parents) {
    let parentDepth = getDepth(parent)
    if (!parentDepth) {
      parentDepth = 0
    }

    if (parentDepth && (!maxParentDepth || maxParentDepth < parentDepth)) {
      maxParentDepth = parentDepth
    }
  }

  if (!maxParentDepth) {
    maxParentDepth = 0
  }

  const updated = setDepth(id, 1 + maxParentDepth)

  if (!updated) {
    return
  }

  // update depth of all children
  const children = redis.smembers(id + '.children')
  if (!children) {
    return
  }

  for (const child of children) {
    updateDepths(child)
    // update the depth of self in child ancestors
    redis.zAddReplaceScore(child + '.ancestors', 1 + maxParentDepth, id)
  }
}

function reCalculateAncestorsFor(ids: Id[]): void {
  table.sort(ids, (a, b) => {
    return (getDepth(a) || 0) < (getDepth(b) || 0)
  })

  for (const id of ids) {
    // clear the ancestors in case of any removed ancestors

    const parents = redis.smembers(id + '.parents')

    let skipAncestorUpdate = false
    if (!parents) {
      skipAncestorUpdate = true
    }

    for (const parent of parents) {
      if (needAncestorUpdates[parent] && !alreadyUpdated[parent]) {
        // if we have a parent that we are about to update later, bail and leave it to them
        skipAncestorUpdate = true
        break
      }
    }

    if (!skipAncestorUpdate) {
      const currentAncestors = redis.zrange(id + '.ancestors')
      redis.del(id + '.ancestors')

      for (const parent of parents) {
        // add all ancestors of parent
        const parentAncestors: string[] = ancestryFromHierarchy(id, parent)

        const reversed: string[] = []
        for (let i = 0; i < parentAncestors.length; i += 2) {
          // reversed[i] = tostring(1 + tonumber(parentAncestors[i + 1]))
          reversed[i] = parentAncestors[i + 1]
          reversed[i + 1] = parentAncestors[i]
        }

        // add root if no ancestors in parent (it's the root)
        if (reversed.length === 0) {
          reversed[0] = '0'
          reversed[1] = 'root'
        }

        redis.zAddMultipleNew(id + '.ancestors', ...reversed)

        // set parent itself into the ancestry
        const parentDepth = getDepth(parent)
        if (parentDepth) {
          // if not root
          redis.zaddNew(id + '.ancestors', parentDepth, parent)
        }
      }

      const ancestors = redis.zrange(id + '.ancestors')

      // if ancestors are the same as before, stop recursion and don't index search
      let eql = true

      if (!ancestors || !currentAncestors) {
        eql = false
      } else if (ancestors.length === 0 || currentAncestors.length === 0) {
        eql = false
      } else if (ancestors.length === currentAncestors.length) {
        for (let i = 0; i < ancestors.length; i++) {
          if (ancestors[i] !== currentAncestors[i]) {
            eql = false
          }
        }
      } else {
        eql = false
      }

      if (!needAncestorUpdates[id] && eql) {
        // do nothing
      } else if (needAncestorUpdates[id] && alreadyUpdated[id] && eql) {
        // do nothing
      } else {
        alreadyUpdated[id] = true

        // add to search
        if (ancestors) {
          const searchStr = joinString(ancestors, ',')
          redis.hset(id, 'ancestors', searchStr)
          addFieldToSearch(id, 'ancestors', searchStr)
        }

        // recurse down the tree if ancestors updated
        const children = redis.smembers(id + '.children')
        if (children && children.length > 0) {
          reCalculateAncestorsFor(children)
        }
      }
    }
  }
}

export function reCalculateAncestors(): void {
  const ids: Id[] = []
  for (const id in needAncestorUpdates) {
    updateDepths(id)
    ids[ids.length] = id
  }

  reCalculateAncestorsFor(ids)
}
