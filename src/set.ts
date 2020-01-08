import { BaseItem, Id, ExternalId, UserType, getTypeFromId } from './schema'
import { SelvaClient } from './'
// type AdvancedSetBaseItem = { [P in keyof BaseItem]: BaseItem[P] | { $default: any, … } }

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

type SetItem = {
  [P in keyof BaseItem]:
    | BaseItem[P]
    | {
        $default: BaseItem[P]
        $value: BaseItem[P]
        $merge: boolean
        $increment: number // check for which things this makes sense
      }
}

type SetOptions = SetItem & {
  $id?: Id
  $merge?: boolean
  $version?: string
  children?: HierarchySet
  parents?: HierarchySet
  ancestors?: HierarchySet
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

async function resetParents(
  client: SelvaClient,
  id: string,
  value: Id[],
  setKey: string
) {
  const ancestorsKey = id + '.ancestors'
  const parents = await client.redis.smembers(id + '.parents')
  if (parents) {
    for (let parent of parents) {
      console.info('REMOVE FROM PARENTS', parent)
      await client.redis.srem(parent + '.children', id)
    }
  }
  await client.redis.del(setKey)
  const newAncestors = []
  await client.redis.del(ancestorsKey)
  for (let parent of value) {
    const childrenKey = parent + '.children'
    await client.redis.sadd(childrenKey, id)
    if (!(await client.redis.exists(parent))) {
      await set(client, { $id: parent })
    } else {
      const ancestors = await client.redis.smembers(parent + '.ancestors')
      newAncestors.push(...ancestors)
    }
    newAncestors.push(parent)
  }
  await client.redis.del(ancestorsKey)
  await client.redis.sadd(ancestorsKey, ...newAncestors)
}

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
      // do it nice
    }
  }
  await client.redis.sadd(setKey, ...value)
}

async function setInner(
  client: SelvaClient,
  id: string,
  value: any,
  fromDefault: boolean,
  field?: string
) {
  if (
    field === 'parents' ||
    field === 'children' ||
    field === 'ancestors' ||
    field === 'externalId' ||
    field === 'auth.role.id'
  ) {
    if (Array.isArray(value)) {
      resetSet(client, field, true, id, value)
    } else {
      // if (value.$value) {
      //   resetSet(client, field, value.$hierarchy || true, id, value)
      // }
      // if (value.$add) {
      //   if (!Array.isArray(value.$add)) {
      //     value.$add = [value.$add]
      //   }
      // }
      // if (value.$remove) {
      //   if (!Array.isArray(value.$remove)) {
      //     value.$remove = [value.$remove]
      //   }
      // }
    }
  } else {
    if (typeof value === 'object' && !Array.isArray(value)) {
      for (let key in value) {
        if (key[0] !== '$') {
          const item = value[key]
          if (typeof item === 'object') {
            if (item.$value) {
              console.log('set $value', item.$value)
            } else if (item.$default) {
              console.log('setnx $default', item.$default)
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
  // field can be 'x.y'
}

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
      console.info('type not defined create it from id')
      payload.type = getTypeFromId(payload.$id)
    }
    console.info('create new item')
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
