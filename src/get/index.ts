import { Item, Id, Language, Type, Text, Field } from '../schema'
import { SelvaClient } from '..'
import getField from './getField'

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

type GetField =
  | {
      $field?: Field
      $inherit?: Inherit
      $list?: List
    }
  | true

type Get<T> = GetField & {
  $default?: T // inherit
}

type GetItem<T = Item> = {
  [P in keyof T]?: true | T[P] extends Item[]
    ? GetItem<T>[]
    : T[P] extends object
    ? GetItem<T[P]> | true
    : T[P] | (Get<T[P]> & MapField)
}

type MapField = GetField & {
  $default?: any
}

type GetOptions = GetItem & {
  $id?: Id
  $version?: string
  $language?: Language
}

// tmp be able to return anythin
// we can also make something else e.g. GetApi (All), Get (Item)
type GetResult<T = Item> = {
  [P in keyof T]?: true | T[P] extends Item[]
    ? GetResult<T>[]
    : T[P] extends Text
    ? T[P] | string
    : T[P] extends object
    ? GetResult<T[P]>
    : T[P]
} & {
  $keys?: string[]
  [key: string]: any
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
      await getField(
        client,
        id,
        props[key],
        field ? field + '.' + key : key,
        result,
        language,
        version
      )
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
