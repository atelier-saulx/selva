import { BaseItem, Id, ExternalId, UserType, getTypeFromId } from './schema'
import { SelvaClient } from './'
import { deleteItem } from './delete'

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
}

// check for which things this makes sense
type SetExtraCounterOptions = {
  $increment?: number
  $decrement?: number
}

type SetItem<T = BaseItem> = {
  [P in keyof T]?: T[P] extends BaseItem[]
    ? SetItem<T>[]
    : T[P] extends object
    ? SetItem<T[P]>
    : T[P] extends number
    ? T[P] | (SetExtraOptions<T[P]> & SetExtraCounterOptions)
    : T[P] | SetExtraOptions<T[P]>
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

async function addToParents(client: SelvaClient, id: string, value: Id[]) {
  for (const parent of value) {
    const childrenKey = parent + '.children'
    await client.redis.sadd(childrenKey, id)
    if (!(await client.redis.exists(parent))) {
      await set(client, { $id: parent })
    }
  }
}

async function resetParents(
  client: SelvaClient,
  id: string,
  value: Id[],
  setKey: string
) {
  const parents = await client.redis.smembers(id + '.parents')
  if (parents) {
    for (const parent of parents) {
      await client.redis.srem(parent + '.children', id)
    }
  }
  await client.redis.del(setKey)
  for (const parent of value) {
    const childrenKey = parent + '.children'
    await client.redis.sadd(childrenKey, id)
    if (!(await client.redis.exists(parent))) {
      await set(client, { $id: parent })
    }
  }
}

async function removeFromParents(client: SelvaClient, id: string, value: Id[]) {
  for (const parent of value) {
    const childrenKey = parent + '.children'
    await client.redis.srem(childrenKey, id)
  }
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
  if (children) {
    for (const child of children) {
      const parentKey = child + '.parents'
      await client.redis.srem(parentKey, id)
      const size = await client.redis.scard(parentKey)
      if (size === 0) {
        await deleteItem(client, child)
      }
    }
  }
  await client.redis.del(setKey)
  for (const child of value) {
    if (!(await client.redis.exists(child))) {
      await set(client, { $id: child })
    }
    await client.redis.sadd(child + '.parents', id)
  }
}

async function addToChildren(client: SelvaClient, id: string, value: Id[]) {
  for (const child of value) {
    await client.redis.sadd(child + '.parents', id)
  }
}

async function removeFromChildren(
  client: SelvaClient,
  id: string,
  value: Id[]
) {}

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
      // do it nice
    }
  }
  await client.redis.sadd(setKey, ...value)
}

// ---------------------------------------------------------------

async function setInner(
  client: SelvaClient,
  id: string,
  value: any,
  fromDefault: boolean,
  field?: string
) {
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
          if (typeof item === 'object') {
            if (item.$value) {
              console.log('set $value', item.$value)
              // overrides increment
            } else if (item.$default) {
              console.log('setnx $default', item.$default)
              if (item.$increment) {
                // handle default as well
              }
            } else if (item.$increment) {
              console.log('incr')
            } else {
              await setInner(
                client,
                id,
                item,
                false,
                field ? field + '.' + key : key
              )
            }
          } else {
            await setInner(
              client,
              id,
              item,
              false,
              field ? field + '.' + key : key
            )
          }
        }
      }
    } else {
      if (fromDefault) {
        await client.redis.hset(id, field, value)
      } else {
        await client.redis.hsetnx(id, field, value)
      }
    }
  }
}

// ---------------------------------------------------------------

// warning when removing all parents from something (by changing children - (default) option to remove automaticly?
async function set(client: SelvaClient, payload: SetOptions): Promise<Id> {
  let exists = false
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
  } else {
    exists = await redis.hexists(payload.$id, 'type')
  }
  if (!exists) {
    if (payload.$id && !payload.type) {
      payload.type = getTypeFromId(payload.$id)
    }
    if (!payload.parents && payload.$id !== 'root') {
      payload.parents = ['root']
    }
    await setInner(client, payload.$id, payload, false)
  } else {
    await setInner(client, payload.$id, payload, false)
  }
  return payload.$id
}

export { set, SetOptions }
