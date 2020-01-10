import { Item, Id, Language, Type } from './schema'
import { SelvaClient } from './'

type Inherit =
  | boolean
  | {
      type: Type[]
    }

type Find = {
  $traverse?: 'descendants' | 'ancestors' | 'children' | 'parents'
}

type List = {
  $range?: [number, number]
  $find?: Find
}

type GetField =
  | {
      $field?: string | string[] // need to map all fields here
      $inherit?: Inherit
      $list?: List
    }
  | true

// can read field options from keys bit hard
// type Field =

type Get<T> = GetField & {
  $default?: T // inherit
}

// TEMP added $merge to Text and Image in baseItem
type GetItem<T = Item> = {
  [P in keyof T]?: T[P] extends Item[]
    ? GetItem<T>[]
    : T[P] extends object
    ? GetItem<T[P]>
    : T[P] | Get<T[P]>
}

// also needs item
type MapField =
  | (GetField & {
      $default?: any // inherit from field - hard to make
    })
  | GetItem

// Get allows every field (maps keys)
// how to combine this ???
// { [key: string]: MapField } &
type GetOptions = GetItem & {
  $id?: Id
  $version?: string
  $language?: Language
}

async function get(client: SelvaClient, props: GetOptions) {
  console.log(props)
}

export { get, GetOptions }
