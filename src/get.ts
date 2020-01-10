import { Item, Id } from './schema'
import { SelvaClient } from './'

type Get =
  | {
      $inherit?: boolean
    }
  | true

// TEMP added $merge to Text and Image in baseItem
type GetItem<T = Item> = {
  [P in keyof T]?: T[P] extends Item[]
    ? GetItem<T>[]
    : T[P] extends object
    ? GetItem<T[P]>
    : T[P] | Get
}

// get allows every field (maps keys)
type GetOptions = Record<string, Get> &
  GetItem & {
    $id?: Id
    $version?: string
  }

async function get(client: SelvaClient, props: GetOptions) {}

export { get, GetOptions }
