import { Text, Item, BaseItem, Id, ExternalId } from './types'

type RedisSetParams =
  | Id[]
  | {
      $hierarchy?: boolean
      $value?: Id[] | Id
      $add?: Id[] | Id
      $delete?: Id[] | Id
    }

function set(
  payload: BaseItem & {
    $id?: string
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
