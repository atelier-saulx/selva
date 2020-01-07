import { BaseItem, Id, ExternalId, UserType } from './schema'

type RedisSetParams =
  | Id[]
  | {
      $hierarchy?: boolean
      $value?: Id[] | Id
      $add?: Id[] | Id
      $delete?: Id[] | Id
    }

// changing ids, default

// { $merge, $default, $value, $increment }

function set(
  payload: BaseItem & {
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
): void {
  console.log('yesh', this.redis)
  if (!payload.$id) {
    console.log('create item')
    if (!payload.type) {
      throw new Error('Cannot create an item if type is not provided')
    }
    // externalID
    // make root on start up?
  }
}

export default set
