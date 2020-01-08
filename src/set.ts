import { BaseItem, Id, ExternalId, UserType } from './schema'

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

type setOptions = SetItem & {
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

// children, parents, ancestors Redis Sets
// fields bla.x (on hash) e.g. title.en
// 
*/

async function set(payload: setOptions) {
  let exists = false
  const redis = this.redis

  if (!payload.$id) {
    if (!payload.type) {
      throw new Error('Cannot create an item if type is not provided')
    }
    if (payload.externalId) {
      payload.$id = this.id({
        type: payload.type,
        externalId: payload.externalId
      })
    } else {
      payload.$id = this.id({ type: payload.type })
    }
  } else {
    const exists = await redis.hexists(payload.$id, 'type')
    console.log(exists)
    // find it!
    // exits
  }

  console.log('exists', await redis.hexists(payload.$id, 'type'))

  if (!exists) {
    console.info('create this')
  }
}

export default set
