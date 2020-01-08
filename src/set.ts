import { BaseItem, Id, ExternalId, UserType } from './schema'
import { SelvaClient } from './'
// type AdvancedSetBaseItem = { [P in keyof BaseItem]: BaseItem[P] | { $default: any, … } }

type RedisSetParams =
  | Id
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
    | ExternalId
    | ExternalId[]
    | {
        $add?: ExternalId[] | ExternalId
        $delete?: ExternalId[] | ExternalId
        $value?: ExternalId[] | ExternalId
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
  if (typeof value === 'object' && !Array.isArray(value)) {
    for (let key in value) {
      if (key[0] !== '$') {
        const item = value[key]
        console.log(' ', key, item)

        // handle parents, children, ancestors

        if (
          field === 'parents' ||
          field === 'children' ||
          field === 'ancestors' ||
          field === 'externalId' ||
          field === 'auth.role.id'
        ) {
          console.log('SET')
        } else if (typeof item === 'object') {
          if (item.$value) {
            console.log('set value', item.$value)
          } else if (item.$default) {
            //   redis.setnx
            console.log('getset default', item.$value)
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
    console.log('hello -->', field, value, fromDefault)
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
    console.info('create new item')
    await setInner(client, payload.$id, payload, false)
  }
}

export { set, SetOptions }
