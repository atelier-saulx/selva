import {
  BaseItem,
  Id,
  ExternalId,
  UserType,
  getTypeFromId,
  Field,
  Layout,
  Component
} from './schema'
import { SelvaClient } from './'
import { deleteItem } from './delete'
import {
  addToAncestors,
  resetAncestors,
  removeFromAncestors
} from './ancestors'
import { arrayIsEqual } from './util'

type RedisSetParams =
  | Id[]
  | {
      $value?: Id[] | Id
      $add?: Id[] | Id
      $delete?: Id[] | Id
    }

type HierarchySet = RedisSetParams & {
  $hierarchy?: boolean
}

type SetExtraOptions<T> = {
  $default?: T
  $value?: T
  $merge?: boolean
  $field?: Field
}

type SetExtraCounterOptions = {
  $increment?: number
}

type SetItem<T = BaseItem> = {
  [P in keyof T]?: T[P] extends BaseItem[]
    ? SetItem<T>[]
    : T[P] extends object
    ? SetItem<T[P]> & SetExtraOptions<T>
    : T[P] extends number
    ? T[P] | (SetExtraOptions<T[P]> & SetExtraCounterOptions)
    : T[P] extends string
    ? T[P] | SetExtraOptions<T[P]>
    : T[P] extends boolean
    ? T[P] | SetExtraOptions<T[P]>
    : T[P] | (T[P] & SetExtraOptions<T[P]>)
}

type SetOptions = SetItem & {
  $id?: Id
  $merge?: boolean
  $version?: string
  children?: HierarchySet
  parents?: HierarchySet
  externalId?:
    | ExternalId[]
    | {
        $add?: ExternalId[] | ExternalId
        $delete?: ExternalId[] | ExternalId
        $value?: ExternalId[]
      }
  auth?: {
    password?: string
    google?: string
    facebook?: string
    role?: {
      id?: RedisSetParams
      type?: UserType
    }
  }
}

// ---------------------------------------------------------------
// addToAncestors
async function addToParents(client: SelvaClient, id: string, value: Id[]) {
  for (const parent of value) {
    const childrenKey = parent + '.children'
    await client.redis.sadd(childrenKey, id)
    if (!(await client.redis.exists(parent))) {
      await set(client, { $id: parent })
    }
  }
  await addToAncestors(client, id, value)
}

async function resetParents(
  client: SelvaClient,
  id: string,
  value: Id[],
  setKey: string
) {
  const parents = await client.redis.smembers(id + '.parents')
  if (arrayIsEqual(parents, value)) {
    return
  }
  for (const parent of parents) {
    await client.redis.srem(parent + '.children', id)
  }
  await client.redis.del(setKey)
  for (const parent of value) {
    await client.redis.sadd(parent + '.children', id)
    if (!(await client.redis.exists(parent))) {
      // add it to root
      await set(client, { $id: parent })
    }
  }

  await resetAncestors(client, id, value, parents)
  // remove item if parents size is 0 - bit shitty but can happen
}

async function removeFromParents(client: SelvaClient, id: string, value: Id[]) {
  for (const parent of value) {
    await client.redis.srem(parent + '.children', id)
  }
  await removeFromAncestors(client, id, value)
}

// ---------------------------------------------------------------
// children field
async function resetChildren(
  client: SelvaClient,
  id: string,
  value: Id[],
  setKey: string
) {
  const children = await client.redis.smembers(setKey)
  if (arrayIsEqual(children, value)) {
    return
  }
  for (const child of children) {
    const parentKey = child + '.parents'
    await client.redis.srem(parentKey, id)
    const size = await client.redis.scard(parentKey)
    if (size === 0) {
      await deleteItem(client, child)
    } else {
      await removeFromAncestors(client, child, [id])
    }
  }
  await client.redis.del(setKey)
  await addToChildren(client, id, value)
}

async function addToChildren(client: SelvaClient, id: string, value: Id[]) {
  for (const child of value) {
    if (!(await client.redis.exists(child))) {
      await set(client, { $id: child, parents: { $add: id } })
    } else {
      await client.redis.sadd(child + '.parents', id)
      await addToAncestors(client, child, [id])
    }
  }
}

async function removeFromChildren(
  client: SelvaClient,
  id: string,
  value: Id[]
) {
  for (const child of value) {
    await client.redis.srem(child + '.parents', id)
    await removeFromAncestors(client, child, [id])
  }
}

// ---------------------------------------------------------------
async function resetSet(
  client: SelvaClient,
  field: string,
  hierarchy: boolean,
  id: string,
  value: Id[]
) {
  const setKey = id + '.' + field
  if (hierarchy) {
    if (field === 'parents') {
      await resetParents(client, id, value, setKey)
    } else if (field === 'children') {
      await resetChildren(client, id, value, setKey)
    }
  } else {
    await client.redis.del(setKey)
  }
  // empty parents means the same as delete item
  await client.redis.sadd(setKey, ...value)
}

async function addToSet(
  client: SelvaClient,
  field: string,
  hierarchy: boolean = true,
  id: string,
  value: Id[]
) {
  const setKey = id + '.' + field
  if (hierarchy) {
    if (field === 'parents') {
      await addToParents(client, id, value)
    } else if (field === 'children') {
      await addToChildren(client, id, value)
    }
  }
  await client.redis.sadd(setKey, ...value)
}

async function removeFromSet(
  client: SelvaClient,
  field: string,
  hierarchy: boolean = true,
  id: string,
  value: Id[]
) {
  const setKey = id + '.' + field
  if (hierarchy) {
    if (field === 'parents') {
      await removeFromParents(client, id, value)
    } else if (field === 'children') {
      await removeFromChildren(client, id, value)
    }
  }
  await client.redis.srem(setKey, ...value)
}

// used for shallow merge
const removeFields = async (
  client: SelvaClient,
  id: string,
  value: object,
  keys: string[],
  field?: string
) => {
  const path = field ? field.split('.') : []
  if (!field) {
    // no field is slightly different
    for (let key in value) {
      if (key[0] !== '$' && typeof value[key] === 'object') {
        removeFields(client, id, value[key], keys, key)
      }
    }
  } else {
    for (let key in value) {
      if (key[0] !== '$') {
        for (const fieldKey of keys) {
          const fields = fieldKey.split('.')
          let removeField = true
          for (let i = 0; i < path.length; i++) {
            if (fields[i] !== path[i]) {
              removeField = false
              break
            }
          }
          if (removeField) {
            removeField = false
            let segment = value
            for (let i = path.length; i < fields.length; i++) {
              segment = segment[fields[i]]
              if (!segment) {
                removeField = true
                break
              }
            }
            if (removeField) {
              await client.redis.hdel(id, fieldKey)
            }
          }
        }
        if (typeof value[key] === 'object') {
          removeFields(client, id, value[key], keys, path.join('.'))
        }
      }
    }
  }
}

// ---------------------------------------------------------------
async function setInner(
  client: SelvaClient,
  id: string,
  value: any,
  fromDefault: boolean,
  field?: string
) {
  if (!fromDefault && value.$merge === false) {
    const keys = await client.redis.hkeys(id)
    await removeFields(client, id, value, keys, field)
  }

  // SET

  if (
    field === 'parents' ||
    field === 'children' ||
    field === 'externalId' ||
    field === 'auth.role.id'
  ) {
    if (Array.isArray(value)) {
      await resetSet(client, field, true, id, value)
    } else {
      const hierarchy = value.$hierarchy === false ? false : true
      if (value.$value) {
        resetSet(client, field, hierarchy, id, value)
      }
      if (value.$add) {
        if (!Array.isArray(value.$add)) {
          value.$add = [value.$add]
        }
        await addToSet(client, field, hierarchy, id, value.$add)
      }
      if (value.$delete) {
        if (!Array.isArray(value.$delete)) {
          value.$delete = [value.$delete]
        }
        await removeFromSet(client, field, hierarchy, id, value.$delete)
      }
    }
  } else {
    if (typeof value === 'object' && !Array.isArray(value)) {
      for (let key in value) {
        if (key[0] !== '$') {
          const item = value[key]
          const nestedField = field ? field + '.' + key : key
          if (typeof item === 'object') {
            if (item.$value) {
              await setInner(client, id, item, false, nestedField)
            } else if (item.$default) {
              if (item.$increment) {
                if (
                  !(await setInner(
                    client,
                    id,
                    item.$default,
                    true,
                    nestedField
                  ))
                ) {
                  await client.redis.hincrby(id, nestedField, item.$increment)
                }
              } else {
                await setInner(client, id, item.$default, true, nestedField)
              }
            } else if (item.$increment) {
              await client.redis.hincrby(id, nestedField, item.$increment)
            } else {
              await setInner(client, id, item, false, nestedField)
            }
          } else {
            await setInner(client, id, item, false, nestedField)
          }
        }
      }
    } else {
      if (Array.isArray(value)) {
        value = JSON.stringify(value)
        // for layouts, handle in get as well
      }
      if (fromDefault) {
        return await client.redis.hsetnx(id, field, value)
      } else {
        return await client.redis.hset(id, field, value)
      }
    }
  }
}

// ---------------------------------------------------------------
async function set(client: SelvaClient, payload: SetOptions): Promise<Id> {
  const redis = client.redis
  if (!payload.$id) {
    if (!payload.type) {
      throw new Error('Cannot create an item if type is not provided')
    }
    const type =
      typeof payload.type === 'string' ? payload.type : payload.type.$value
    if (
      (payload.externalId && typeof payload.externalId === 'string') ||
      Array.isArray(payload.externalId)
    ) {
      payload.$id = client.id({
        type,
        externalId: payload.externalId
      })
    } else {
      payload.$id = client.id({ type })
    }
  }

  let exists = (await redis.hexists(payload.$id, 'type')) || false

  if (!exists) {
    if (payload.$id && !payload.type) {
      payload.type = getTypeFromId(payload.$id)
    }
    if (!payload.parents && payload.$id !== 'root') {
      payload.parents = { $add: 'root' }
    }
  }

  await setInner(client, payload.$id, payload, false)
  return payload.$id
}

export { set, SetOptions }
