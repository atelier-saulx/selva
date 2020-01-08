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
    const setKey = id + '.' + field
    if (Array.isArray(value)) {
      await client.redis.del(setKey)
      await client.redis.sadd(setKey, ...value)
      if (field === 'parents') {
        const ancestorsKey = id + '.ancestors'
        const ancestors = await client.redis.smembers(ancestorsKey)
        // for each in ancestors remove from children
        await client.redis.del(ancestorsKey)
        for (let parent of value) {
          const childrenKey = parent + '.children'
          await client.redis.sadd(childrenKey, id)
          if (!(await client.redis.exists(parent))) {
            await set(client, { $id: parent })
          }
        }

        await client.redis.del(setKey)
      } else if (field === 'children') {
      }
    } else {
      // $add, $remove, $value, $hierarchy
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
      console.log('SET FIELD -->', field, value, fromDefault)
      if (fromDefault) {
        await client.redis.hset(id, field, value)
      } else {
        await client.redis.hsetnx(id, field, value)
      }
    }
  }
  // field can be 'x.y'
}

async function set(client: SelvaClient, payload: SetOptions) {
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
  }
}

export { set, SetOptions }
