import { SetOptions } from '~selva/setTypes'
import { Id } from '~selva/schema'
import * as redis from '../redis'
import { arrayIsEqual } from '../util'
import { reCalculateAncestors } from './ancestors'

function getSetKey(id: string, field: string): string {
  return id + '.' + field
}

type FnModify = (payload: SetOptions & { $id: string }) => Id

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

  redis.del(setKey)
  redis.sadd(setKey, ...value)
}

export function addToSet(
  id: string,
  field: string,
  value: Id[],
  modify: FnModify,
  hierarchy: boolean = true
): void {
  const setKey = getSetKey(id, field)
  if (hierarchy) {
    if (field === 'parents') {
      addToParents(id, value, modify)
    } else if (field === 'children') {
      addToChildren(id, value, modify)
    }
  }

  redis.sadd(setKey, ...value)
}

export function removeFromSet(
  id: string,
  field: string,
  value: Id[],
  hierarchy: boolean = true
): void {
  const setKey = getSetKey(id, field)
  if (hierarchy) {
    if (field === 'parents') {
      removeFromParents(id, value)
    } else if (field === 'children') {
      removeFromChildren(id, value)
    }
  }
  redis.srem(setKey, ...value)
}

export function resetParents(
  id: string,
  setKey: string,
  value: Id[],
  modify: FnModify
): void {
  // bail if parents are unchanged
  const parents = redis.smembers(id + '.parents')
  if (arrayIsEqual(parents, value)) {
    return
  }

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

  reCalculateAncestors(id, parents)
}

export function addToParents(id: string, value: Id[], modify: FnModify): void {
  for (const parent of value) {
    const childrenKey = parent + '.children'
    redis.sadd(childrenKey, id)
    if (!redis.exists(parent)) {
      modify({ $id: parent })
    }
  }

  const parents = redis.smembers(id + '.parents')
  reCalculateAncestors(id, parents)
}

export function removeFromParents(id: string, value: Id[]): void {
  for (const parent of value) {
    redis.srem(parent + '.children', id)
  }

  const parents = redis.smembers(id + '.parents')
  reCalculateAncestors(id, parents)
}

export function addToChildren(id: string, value: Id[], modify: FnModify): void {
  for (const child of value) {
    if (!redis.exists(child)) {
      modify({ $id: child, parents: { $add: id } })
    } else {
      redis.sadd(child + '.parents', id)
      reCalculateAncestors(child, redis.smembers(child + '.parents'))
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
  if (arrayIsEqual(children, value)) {
    return
  }
  for (const child of children) {
    const parentKey = child + '.parents'
    redis.srem(parentKey, id)
    const size = redis.scard(parentKey)
    if (size === 0) {
      // TODO: deleteItem(child)
    } else {
      reCalculateAncestors(child, redis.smembers(parentKey))
    }
  }
  redis.del(setKey)
  addToChildren(id, value, modify)
}

export function removeFromChildren(id: string, value: Id[]): void {
  for (const child of value) {
    redis.srem(child + '.parents', id)
    reCalculateAncestors(child, redis.smembers(child + '.parents'))
  }
}
