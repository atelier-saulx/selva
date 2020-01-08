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
        $default: any
        $value: any
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

function set(payload: setOptions): void {
  if (!payload.$id) {
    console.log('create item', payload)
    if (!payload.type) {
      throw new Error('Cannot create an item if type is not provided')
    }

    if (!payload.parents) {
      console.info('no parents provided add to root')
      payload.parents = ['root']
    }

    // externalID
    // make root on start up?
  }
}

export default set
