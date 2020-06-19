import { id as genId } from '../id'
import { SetOptions } from '~selva/set/types'
import { Id } from '~selva/schema/index'
import * as redis from '../redis'
import { markForAncestorRecalculation } from './ancestors'
import { deleteItem } from './delete'
import sendEvent from './events'
import { log, info, configureLogger } from '../logger'
import globals from '../globals'
import { arrayIsEqual } from '../util'
import checkSource from './source'
import { markUpdated } from './timestamps'

type FnModify = (payload: SetOptions) => Id | null

function getSetKey(id: string, field: string): string {
  return id + '.' + field
}

export function resetSet(
  id: string,
  field: string,
  value: Id[],
  modify: FnModify,
  hierarchy: boolean = true,
  noRoot: boolean = false,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): void {
  const setKey = getSetKey(id, field)
  const current = redis.smembers(setKey)
  if (arrayIsEqual(current, value)) {
    return
  }

  if (hierarchy) {
    if (field === 'parents') {
      resetParents(id, setKey, value, modify, noRoot, source)
    } else if (field === 'children') {
      value = resetChildren(id, setKey, value, modify, noRoot, source)
    } else if (field === 'aliases') {
      resetAlias(id, value)
    } else {
      redis.del(setKey)
    }
  } else {
    redis.del(setKey)
  }

  if (value.length === 0) {
    redis.del(setKey)
  } else {
    redis.sadd(setKey, ...value)
  }

  markUpdated(id)
  sendEvent(id, field, 'update')
}

export function addToSet(
  id: string,
  field: string,
  value: Id[],
  modify: FnModify,
  hierarchy: boolean = true,
  noRoot: boolean = false,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): void {
  if (value.length === 0) {
    return
  }

  const setKey = getSetKey(id, field)

  if (hierarchy) {
    if (field === 'parents') {
      addToParents(id, value, modify, noRoot, source)
    } else if (field === 'children') {
      value = addToChildren(id, value, modify, noRoot, source)
    } else if (field === 'aliases') {
      addAlias(id, value)
    }
  }

  if (value.length > 0) {
    const added = redis.sadd(setKey, ...value)
    if (added > 0) {
      markUpdated(id)
      sendEvent(id, field, 'update')
    }
  }
}

export function removeFromSet(
  id: string,
  field: string,
  value: Id[],
  hierarchy: boolean = true,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): void {
  if (value.length === 0) {
    return
  }

  const setKey = getSetKey(id, field)
  redis.srem(setKey, ...value)

  if (hierarchy) {
    if (field === 'parents') {
      removeFromParents(id, value, source)
    } else if (field === 'children') {
      removeFromChildren(id, value, source)
    } else if (field === 'aliases') {
      removeAlias(id, value)
    }
  }

  markUpdated(id)
  sendEvent(id, field, 'update')
}

export function resetParents(
  id: string,
  setKey: string,
  value: Id[],
  modify: FnModify,
  noRoot: boolean = false,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): void {
  // TODO: can be passed from "above"
  const parents = redis.smembers(id + '.parents')

  // clean up existing parents
  for (const parent of parents) {
    if (checkSource(parent, 'children', source)) {
      redis.srem(parent + '.children', id)
    }
  }

  redis.del(setKey)

  // add new parents
  for (const parent of value) {
    // recurse if necessary
    if (!redis.exists(parent)) {
      if (noRoot) {
        modify({ $id: parent, parents: [] })
      } else {
        modify({ $id: parent })
      }
    }

    redis.sadd(parent + '.children', id)
    markUpdated(id)
    sendEvent(parent, 'children', 'update')
  }

  markForAncestorRecalculation(id)
}

export function addToParents(
  id: string,
  value: Id[],
  modify: FnModify,
  noRoot: boolean = false,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): void {
  let numAdded = 0
  for (const parent of value) {
    const childrenKey = parent + '.children'

    if (checkSource(parent, 'children', source)) {
      const added = redis.sadd(childrenKey, id)
      numAdded += added
      if (added === 1) {
        if (!redis.exists(parent)) {
          if (noRoot) {
            modify({ $id: parent, parents: [] })
          } else {
            modify({ $id: parent })
          }
        }

        markUpdated(id)
        sendEvent(parent, 'children', 'update')
      }
    }
  }

  if (numAdded > 0) {
    markForAncestorRecalculation(id)
  }
}

export function removeFromParents(
  id: string,
  value: Id[],
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): void {
  for (const parent of value) {
    if (checkSource(parent, 'children', source)) {
      redis.srem(parent + '.children', id)
    }
  }

  markForAncestorRecalculation(id)
}

export function addToChildren(
  id: string,
  value: Id[],
  modify: FnModify,
  noRoot: boolean = false,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): Id[] {
  const result: string[] = []
  for (let i = 0; i < value.length; i++) {
    let child = value[i]
    // if the child is an object
    // automatic creation is attempted
    if (type(child) === 'table') {
      if (!(<any>child).parents) {
        ;(<any>child).parents = { $add: [id] }
      }

      if ((<any>child).$id || (<any>child).$alias) {
        child = modify(<any>child) || ''
      } else if ((<any>child).type !== null) {
        ;(<any>child).$id = genId({ type: (<any>child).type })
        child = modify(<any>child) || ''
      } else {
        // FIXME: throw new Error('No type or id provided for dynamically created child')
        child = ''
      }
    }

    result[i] = child

    if (child !== '') {
      if (!redis.exists(child)) {
        // if (noRoot) {
        //   modify({ $id: child, parents: [] })
        // } else {
        //   modify({ $id: child })
        // }
        modify({ $id: child, parents: [id] })
      }

      if (checkSource(child, 'parents', source)) {
        const added = redis.sadd(child + '.parents', id)
        if (added === 1) {
          markForAncestorRecalculation(child)
          markUpdated(id)
          sendEvent(child, 'parents', 'update')
        }
      }
    }
  }

  // if (
  //   globals.$_batchOpts &&
  //   globals.$_batchOpts.refField &&
  //   globals.$_batchOpts.refField.resetReference === 'children'
  // ) {
  //   if (globals.$_batchOpts.refField.last) {
  //     const batchId = globals.$_batchOpts.batchId
  //     const bufferedChildren = redis.smembers(
  //       `___selva_reset_children:${batchId}`
  //     )

  //     // run cleanup at the end of the partial batch that processes large reference arrays
  //     for (const child of bufferedChildren) {
  //       const parentKey = child + '.parents'
  //       const size = redis.scard(parentKey)
  //       if (size === 0) {
  //         deleteItem(child)
  //       }
  //     }

  //     redis.del(`___selva_reset_children:${batchId}`)
  //   }
  // }

  return result
}

export function addAlias(id: string, value: Id[]): void {
  for (const v of value) {
    const current = redis.hget('___selva_aliases', v)
    if (current !== id) {
      redis.srem(current + '.aliases', v)
    }

    redis.hset('___selva_aliases', v, id)
  }
}

export function resetChildren(
  id: string,
  setKey: string,
  value: Id[],
  modify: FnModify,
  noRoot: boolean = false,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): Id[] {
  let batchId = null
  // if (
  //   globals.$_batchOpts &&
  //   globals.$_batchOpts.refField &&
  //   globals.$_batchOpts.refField.resetReference === 'children'
  // ) {
  //   batchId = globals.$_batchOpts.batchId
  // }

  const children = redis.smembers(setKey)
  if (arrayIsEqual(children, value)) {
    return children
  }

  for (const child of children) {
    const parentKey = child + '.parents'
    redis.srem(parentKey, id)
  }

  redis.del(setKey)
  const newChildren = addToChildren(id, value, modify, noRoot, source)
  for (const child of children) {
    const parentKey = child + '.parents'
    // bit special but good for perf to skip this in batching mode
    const size = batchId ? 1 : redis.scard(parentKey)
    if (size === 0) {
      deleteItem(child)
    } else {
      markForAncestorRecalculation(child)
    }
  }

  if (batchId) {
    if (children && children.length >= 1) {
      redis.sadd(`___selva_reset_children:${batchId}`, ...children)
      redis.expire(`___selva_reset_children:${batchId}`, 60 * 15) // expires in 15 minutes
    }
  }

  return newChildren
}

export function resetAlias(id: string, value: Id[]): void {
  const current = redis.smembers(id + '.aliases')
  if (current) {
    for (const v of current) {
      redis.hdel('___selva_aliases', v)
    }
  }

  redis.del(id + '.aliases')
  addAlias(id, value)
}

export function removeFromChildren(
  id: string,
  value: Id[],
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): void {
  for (const child of value) {
    if (checkSource(child, 'parents', source)) {
      redis.srem(child + '.parents', id)
      markForAncestorRecalculation(child)
    }
  }
}

export function removeAlias(_id: string, value: Id[]): void {
  for (const v of value) {
    redis.hdel('___selva_aliases', v)
  }
}
