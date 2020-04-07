import { Id, Schema } from '../../../src/schema/index'
import * as redis from '../redis'
import { joinString } from '../util'
import { addFieldToSearch } from './search'
import sendEvent from './events'
import { getSchema } from '../schema/index'
import { getTypeFromId } from '../typeIdMapping'
import * as logger from '../logger'
import globals from '../globals'

const schema: Schema = getSchema()
const needAncestorUpdates: Record<Id, true> = {}
const alreadyUpdated: Record<Id, true> = {}
const depthMap: Record<Id, number> = {}

type IncludeAncestryRule = { includeAncestryWith: string[] }
type ExcludeAncestryRule = { excludeAncestryWith: string[] }
type AncestryRule = IncludeAncestryRule | ExcludeAncestryRule | false

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
): rule is ExcludeAncestryRule {
  if (!rule) {
    return false
  }

  return !!(<any>rule).excludeAncestryWith
}

function includeAncestors(
  includedTypes: string[],
  ancestors: string[]
): string[] {
  for (let i = 0; i < ancestors.length; i += 2) {
    for (const includedTypeName of includedTypes) {
      // TODO: we can compare just the prefixes here
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
  for (let i = 0; i < ancestors.length; i += 2) {
    for (const excludedTypeName of excludedTypes) {
      if (getTypeFromId(ancestors[i]) === excludedTypeName) {
        return []
      }
    }
  }

  return ancestors
}

function ancestryFromHierarchy(id: Id, parent: Id): string[] {
  if (parent === 'root') {
    return ['root', '0']
  }

  // can't map, should never happen though
  if (!schema.prefixToTypeMapping) {
    const finalAncestors = redis.zrangeWithScores(parent + '.ancestors')
    const depth = getDepth(parent)
    finalAncestors[finalAncestors.length] = parent
    finalAncestors[finalAncestors.length] = tostring(depth)
    return finalAncestors
  }

  const typeName: string = schema.prefixToTypeMapping[id.substring(0, 2)]
  const parentTypeName: string =
    schema.prefixToTypeMapping[parent.substring(0, 2)]

  const hierarchy = schema.types[typeName].hierarchy

  if (!hierarchy) {
    const finalAncestors = redis.zrangeWithScores(parent + '.ancestors')
    const depth = getDepth(parent)
    finalAncestors[finalAncestors.length] = parent
    finalAncestors[finalAncestors.length] = tostring(depth)
    return finalAncestors
  }

  let foundRule: AncestryRule | null = null

  for (const ruleTypeName in hierarchy) {
    if (ruleTypeName === parentTypeName) {
      foundRule = hierarchy[ruleTypeName]
      break
    }
  }

  if (type(foundRule) === 'boolean' && foundRule === false) {
    return []
  }

  let finalAncestors: string[] = []
  const parentsOfParent = redis.smembers(parent + '.parents')
  for (const parentOfParent of parentsOfParent) {
    let ancestors = redis.zrangeWithScores(parentOfParent + '.ancestors')

    // set parent of parent itself into the ancestry
    const depth = getDepth(parentOfParent)
    ancestors[ancestors.length] = parentOfParent
    ancestors[ancestors.length] = tostring(depth)

    if (isIncludeAncestryRule(foundRule)) {
      ancestors = includeAncestors(foundRule.includeAncestryWith, ancestors)
    } else if (isExcludeAncestryRule(foundRule)) {
      ancestors = excludeAncestors(foundRule.excludeAncestryWith, ancestors)
    } else if (hierarchy.$default) {
      const rule = hierarchy.$default
      if (isIncludeAncestryRule(rule)) {
        ancestors = includeAncestors(rule.includeAncestryWith, ancestors)
      } else {
        ancestors = excludeAncestors(rule.excludeAncestryWith, ancestors)
      }
    }

    // include ancestors in result
    for (let i = 0; i < ancestors.length; i++) {
      finalAncestors[finalAncestors.length] = ancestors[i]
    }
  }

  // set parent itself into the ancestry
  const depth = getDepth(parent)
  finalAncestors[finalAncestors.length] = parent
  finalAncestors[finalAncestors.length] = tostring(depth)

  return finalAncestors
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
    return 0
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

function reCalculateAncestorsFor(ids: Id[]): void {
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

    let maxParentDepth = 0
    if (!skipAncestorUpdate) {
      // TODO: compare ancestor string maybe for extra fast?
      const currentAncestors = redis.zrange(id + '.ancestors')
      redis.del(id + '.ancestors')

      for (const parent of parents) {
        const parentDepth = getDepth(parent)
        if (parentDepth && parentDepth > maxParentDepth) {
          maxParentDepth = parentDepth
        }

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
      }

      setDepth(id, maxParentDepth + 1)

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

        // send event
        sendEvent(id, 'ancestors', 'update')

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
  let ids: Id[] = []
  const newIds: Id[] = []
  for (const id in needAncestorUpdates) {
    newIds[newIds.length] = id
  }

  if (globals.$_batchOpts) {
    const { batchId } = globals.$_batchOpts

    logger.info('IS BATCH, STATUS?', globals.$_batchOpts.last)
    if (!globals.$_batchOpts.last) {
      if (newIds.length > 0) {
        redis.sadd(`___selva_ancestors_batch:${batchId}`, ...newIds)
        redis.expire(`___selva_ancestors_batch:${batchId}`, 60 * 1) // expires in 15 minutes 5
      }

      logger.info(
        'MID BATCH, SKIPPING ANCESTOR RECALC WITH AMONUT',
        newIds.length
      )
      return
    }

    ids = redis.smembers(`___selva_ancestors_batch:${batchId}`) || []
    logger.info('ADDING OLD BATCH IDS FOR ANCESTOR RECALC', ids.length)
    for (const id of newIds) {
      ids[ids.length] = id
    }
  } else {
    ids = newIds
  }

  logger.info('RECALC ANCESTORS', ids.length)
  reCalculateAncestorsFor(ids)
}
