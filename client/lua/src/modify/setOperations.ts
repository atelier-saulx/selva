import { id as genId } from '../id'
import { SetOptions } from '~selva/set/types'
import { Id } from '~selva/schema/index'
import * as redis from '../redis'
import { markForAncestorRecalculation } from './ancestors'
import { deleteItem } from './delete'
import sendEvent from './events'
import { log, info, configureLogger } from '../logger'

type FnModify = (payload: SetOptions) => Id | null

function getSetKey(id: string, field: string): string {
  return id + '.' + field
}

export function resetSet(
  id: string,
  field: string,
  value: Id[],
  modify: FnModify,
  hierarchy: boolean = true
): void {
  const setKey = getSetKey(id, field)

  if (hierarchy) {
    if (field === 'parents') {
      resetParents(id, setKey, value, modify)
    } else if (field === 'children') {
      value = resetChildren(id, setKey, value, modify)
    } else if (field === 'aliases') {
      resetAlias(id, value)
    }
  } else {
    redis.del(setKey)
  }

  if (value.length === 0) {
    redis.del(setKey)
  } else {
    redis.sadd(setKey, ...value)
  }
}

export function addToSet(
  id: string,
  field: string,
  value: Id[],
  modify: FnModify,
  hierarchy: boolean = true
): void {
  const setKey = getSetKey(id, field)
  redis.sadd(setKey, ...value)

  if (hierarchy) {
    if (field === 'parents') {
      addToParents(id, value, modify)
    } else if (field === 'children') {
      addToChildren(id, value, modify)
    } else if (field === 'aliases') {
      addAlias(id, value)
    }
  }
}

export function removeFromSet(
  id: string,
  field: string,
  value: Id[],
  hierarchy: boolean = true
): void {
  const setKey = getSetKey(id, field)
  redis.srem(setKey, ...value)

  if (hierarchy) {
    if (field === 'parents') {
      removeFromParents(id, value)
    } else if (field === 'children') {
      removeFromChildren(id, value)
    } else if (field === 'aliases') {
      removeAlias(id, value)
    }
  }
}

export function resetParents(
  id: string,
  setKey: string,
  value: Id[],
  modify: FnModify
): void {
  const parents = redis.smembers(id + '.parents')
  // bail if parents are unchanged
  // needs to be commented for now as we set before recalculating ancestors
  // this will likely change as we optimize ancestor calculation
  // if (arrayIsEqual(parents, value)) {
  //   return
  // }

  // clean up existing parents
  for (const parent of parents) {
    redis.srem(parent + '.children', id)
  }

  redis.del(setKey)

  // add new parents
  for (const parent of value) {
    // recurse if necessary
    if (!redis.exists(parent)) {
      modify({ $id: parent })
    }

    redis.sadd(parent + '.children', id)
    sendEvent(parent, 'children', 'update')
  }

  markForAncestorRecalculation(id)
}

export function addToParents(id: string, value: Id[], modify: FnModify): void {
  for (const parent of value) {
    const childrenKey = parent + '.children'
    redis.sadd(childrenKey, id)
    sendEvent(parent, 'children', 'update')
    if (!redis.exists(parent)) {
      modify({ $id: parent })
    }
  }

  markForAncestorRecalculation(id)
}

export function removeFromParents(id: string, value: Id[]): void {
  for (const parent of value) {
    redis.srem(parent + '.children', id)
  }

  markForAncestorRecalculation(id)
}

export function addToChildren(id: string, value: Id[], modify: FnModify): Id[] {
  const result: string[] = []
  for (let i = 0; i < value.length; i++) {
    let child = value[i]
    // if the child is an object
    // automatic creation is attempted
    if (type(child) === 'table') {
      if ((<any>child).$id) {
        child = modify(<any>child) || ''
      } else if (!(<any>child).$id && (<any>child).type !== null) {
        ;(<any>child).$id = genId({ type: (<any>child).type })
        child = modify(<any>child) || ''
      } else {
        // FIXME: throw new Error('No type or id provided for dynamically created child')
        child = ''
      }
    }

    result[i] = child

    if (child !== '') {
      redis.sadd(child + '.parents', id)

      if (!redis.exists(child)) {
        modify({ $id: child, parents: { $add: id } })
      }

      sendEvent(child, 'parents', 'update')
      markForAncestorRecalculation(child)
    }
  }

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
  modify: FnModify
): Id[] {
  const children = redis.smembers(setKey)

  for (const child of children) {
    const parentKey = child + '.parents'
    redis.srem(parentKey, id)
  }

  redis.del(setKey)
  const newChildren = addToChildren(id, value, modify)
  for (const child of children) {
    const parentKey = child + '.parents'
    const size = redis.scard(parentKey)
    if (size === 0) {
      deleteItem(child)
    } else {
      markForAncestorRecalculation(child)
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

  addAlias(id, value)
}

export function removeFromChildren(id: string, value: Id[]): void {
  for (const child of value) {
    redis.srem(child + '.parents', id)
    markForAncestorRecalculation(child)
  }
}

export function removeAlias(_id: string, value: Id[]): void {
  for (const v of value) {
    redis.hdel('___selva_aliases', v)
  }
}
