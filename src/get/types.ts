import { Language, Field, Item, Type, Id } from '../schema'

export type Inherit =
  | boolean
  | {
      $type?: Type | Type[]
      $name?: string | string[]
      // id?: Id | Id[]
      $item?: Type | Type[]
    }

export type Find = {
  $traverse?: 'descendants' | 'ancestors' | 'children' | 'parents'
}

export type List = {
  $range?: [number, number]
  $find?: Find
}

export type GetField<T> = {
  $field?: Field
  $inherit?: Inherit
  $list?: List
  $default?: T
}

// update $language for default + text (algebraic)
export type GetItem<T = Item> = {
  [P in keyof T]?: T[P] extends Item[]
    ? GetItem<T>[] | true
    : T[P] extends Text
    ? (GetItem<T[P]> & GetField<T | string>) | true
    : T[P] extends object
    ? (GetItem<T[P]> & GetField<T>) | true
    : T[P] extends number
    ? T[P] | GetField<T[P]> | true
    : T[P] extends string
    ? T[P] | GetField<T[P]> | true
    : T[P] extends boolean
    ? T[P] | GetField<T[P]>
    : (T[P] & GetField<T[P]>) | true
} &
  GetField<T> & {
    [key: string]: any
  }

// but explodes :D missing true somwhere

// type Get<T> =
//   | (GetField & {
//       $default?: T // inherit
//     })
//   | true

// & MapField
// type MapField = GetField & {
//   $default?: any
// }

export type GetOptions = GetItem & {
  $id?: Id
  $version?: string
  $language?: Language
}

// tmp be able to return anything
// this is the result
// we can also make something else e.g. GetApi (All), Get (Item)
export type GetResult<T = Item> = {
  [P in keyof T]?: T[P] extends Item[]
    ? GetResult<T>[]
    : T[P] extends Text
    ? T[P] | string
    : T[P] extends object
    ? GetResult<T[P]>
    : T[P]
} & {
  // $keys?: string[]
  [key: string]: any
}
