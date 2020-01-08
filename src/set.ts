import { BaseItem, Id, ExternalId, UserType } from './schema'
import { SelvaClient } from './'
// type AdvancedSetBaseItem = { [P in keyof BaseItem]: BaseItem[P] | { $default: any, … } }

type RedisSetParams =
  | Id[]
  | {
      $hierarchy?: boolean
      $value?: Id[] | Id
      $add?: Id[] | Id
      $delete?: Id[] | Id
    }

// type AdvancedSetBaseItem = { [P in keyof BaseItem]: BaseItem[P] | { $default: any, … } }
// type Nullable<T> = { [P in keyof T]: T[P] | null }

// changing ids, default
// { $merge, $default, $value, $increment }

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
  children?: RedisSetParams
  parents?: RedisSetParams
  ancestors?: RedisSetParams
  externalId?:
    | ExternalId
    | ExternalId[]
    | {
        $add?: ExternalId[] | ExternalId
        $delete?: ExternalId[] | ExternalId
      }
  auth?: {
    password?: string
    google?: string
    facebook?: string
    role?: {
      id?:
        | Id
        | Id[]
        | {
            $add?: Id[] | Id
            $delete?: Id[] | Id
          }
      type?: UserType
    }
  }
}

/*
- storage setup
hash (id)

// id.children
// id.parents
// id.ancestors 

// children, parents, ancestors Redis Sets
// fields bla.x (on hash) e.g. title.en
// 
*/

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
    // redis
    // const exists = await redis.hexists(payload.$id, 'type')
    // console.log(exists)
    // find it!
    // exits
  }

  console.log(await redis.dbsize())

  console.log('set', await redis.set('flurp', 100))

  console.log('decr', await redis.decr('flurp'))

  console.log('incr', await redis.incr('flurp'))

  console.log('decrby', await redis.decrby('flurp', 10))

  console.log('incrby', await redis.incrby('flurp', 10))

  console.log('get', await redis.get('flurp'))

  console.log('del', await redis.del('flurp'))

  console.log('get', await redis.get('flurp'))

  console.log('xxxx', await redis.psubscribe('flurp'))

  if (!exists) {
    console.info('create this')
  }
}

export { set, SetOptions }
