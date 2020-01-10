import { Item, Id, Language, Type } from './schema'
import { SelvaClient } from './'

type Inherit =
  | boolean
  | {
      type: Type[]
    }

// can read field options from keys bit hard
// type Field =

type MapField = {
  $default?: any // inherit from field - hard to make
  $field: string | string[] // need to map all fields here
  $inherit?: Inherit
}

type Get<T> =
  | {
      $default?: T // inherit
      $field?: string | string[] // need to map all fields here
      $inherit?: Inherit
    }
  | true

// TEMP added $merge to Text and Image in baseItem
type GetItem<T = Item> = {
  [P in keyof T]?: T[P] extends Item[]
    ? GetItem<T>[]
    : T[P] extends object
    ? GetItem<T[P]>
    : T[P] | Get<T[P]>
}

// Get allows every field (maps keys)
type GetOptions = Record<string, MapField> &
  GetItem & {
    $id?: Id
    $version?: string
    $language?: Language
  }

async function get(client: SelvaClient, props: GetOptions) {}

export { get, GetOptions }
