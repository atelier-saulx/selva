import { Item, Id, Language, Type, Text, Field, languages } from '../schema'
import { SelvaClient } from '..'
import getField from './getField'
import { getNestedField, setNestedResult } from './nestedFields'

type Inherit =
  | boolean
  | {
      type?: Type | Type[]
      name?: string | string[]
      id?: Id | Id[]
      $item?: Type | Type[]
    }

type Find = {
  $traverse?: 'descendants' | 'ancestors' | 'children' | 'parents'
}

type List = {
  $range?: [number, number]
  $find?: Find
}

type GetField<T> = {
  $field?: Field
  $inherit?: Inherit
  $list?: List
  $default?: T
}

type GetItem<T = Item> = {
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
  GetField<T>

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

type GetOptions = GetItem & {
  $id?: Id
  $version?: string
  $language?: Language
}

// tmp be able to return anything
// this is the result
// we can also make something else e.g. GetApi (All), Get (Item)
type GetResult<T = Item> = {
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

const isEmpty = (value: any, def: any): boolean => {
  if (value === null || value === undefined || value === '') {
    return true
  } else if (Array.isArray(value) && value.length === 0 && Array.isArray(def)) {
    return true
  } else if (typeof value === 'object') {
    for (const key in value) {
      if (!isEmpty(value[key], def)) {
        return false
      }
    }
    return true
  }
  return false
}

export async function getInner(
  client: SelvaClient,
  props: GetItem,
  result: GetResult,
  id: Id,
  field?: string,
  language?: Language,
  version?: string
): Promise<void> {
  for (let key in props) {
    // handle all special cases here
    if (key[0] !== '$') {
      const f = field ? field + '.' + key : key

      if (props[key] === true) {
        await getField(client, id, f, result, language, version)
      } else {
        await getInner(client, props[key], result, id, f, language, version)
      }
    }
  }

  if (props.$default) {
    await getField(client, id, field, result, language, version)
    const value = getNestedField(result, field)
    if (isEmpty(value, props.$default)) {
      setNestedResult(result, field, props.$default)
    }
  }
}

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const result: GetResult = {}
  const { $version: version, $id: id, $language: language } = props
  if (id) {
    await getInner(client, props, result, id, undefined, language)
  } else {
    // only find
  }
  return result
}

export { get, GetOptions, GetResult }
