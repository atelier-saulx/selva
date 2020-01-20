import { id as genId } from '../id'
import { SetOptions } from '~selva/setTypes'
import { Id } from '~selva/schema'
import * as redis from '../redis'
import { reCalculateAncestors } from './ancestors'
import { deleteItem } from './delete'

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
      resetChildren(id, setKey, value, modify)
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
    redis.sadd(parent + '.children', id)
    // recurse if necessary
    if (redis.exists(parent)) {
      modify({ $id: parent })
    }
  }

  reCalculateAncestors(id, value)
}

export function addToParents(id: string, value: Id[], modify: FnModify): void {
  for (const parent of value) {
    const childrenKey = parent + '.children'
    redis.sadd(childrenKey, id)
    if (!redis.exists(parent)) {
      modify({ $id: parent })
    }
  }

  reCalculateAncestors(id)
}

export function removeFromParents(id: string, value: Id[]): void {
  for (const parent of value) {
    redis.srem(parent + '.children', id)
  }

  reCalculateAncestors(id)
}

export function addToChildren(id: string, value: Id[], modify: FnModify): void {
  for (let child of value) {
    // if the child is an object
    // automatic creation is attempted
    if (type(child) === 'table') {
      if (!(<any>child).$id) {
        modify(<any>child)
      } else if ((<any>child).type !== null) {
        ;(<any>child).$id = genId({ type: (<any>child).type })
        modify(<any>child)
      } else {
        // FIXME: throw new Error('No type or id provided for dynamically created child')
      }
      child = child = (child as any).$id
    }

    if (!redis.exists(child)) {
      modify({ $id: child, parents: { $add: id } })
    } else {
      redis.sadd(child + '.parents', id)
      reCalculateAncestors(child)
    }
  }
}

export function resetChildren(
  id: string,
  setKey: string,
  value: Id[],
  modify: FnModify
): void {
  const children = redis.smembers(setKey)
  // if (arrayIsEqual(children, value)) {
  //   return
  // }
  for (const child of children) {
    const parentKey = child + '.parents'
    redis.srem(parentKey, id)
    const size = redis.scard(parentKey)
    if (size === 0) {
      deleteItem(child)
    } else {
      reCalculateAncestors(child)
    }
  }
  redis.del(setKey)
  addToChildren(id, value, modify)
}

export function removeFromChildren(id: string, value: Id[]): void {
  for (const child of value) {
    redis.srem(child + '.parents', id)
    reCalculateAncestors(child)
  }
}
