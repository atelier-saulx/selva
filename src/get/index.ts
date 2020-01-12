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

// bla.x , bla.$x
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
  $keys?: string[]
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

// fn getLanguage (title, description, article)

export async function getInner(
  client: SelvaClient,
  props: GetItem,
  result: GetResult,
  id: Id,
  field?: string,
  languge?: Language,
  version?: string
): Promise<void> {
  for (let key in props) {
    if (key[0] !== '$') {
      if (props[key] === true) {
        await getField(client, id, key, result, languge, version)
      } else {
        console.log('NOT IMPLEMENTED OBJECT', key)
      }
    }
  }
}

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const result: GetResult = {}
  const { $version: version, $id: id, $language: languge } = props
  if (id) {
    await getInner(client, props, result, id)
  } else {
    // only find
  }

  // this field is used as a cache
  if (result.$keys) {
    if (!props.$keys) {
      delete result.$keys
    }
  } else if (props.$keys) {
    result.$keys = await client.redis.hkeys(id)
  }

  return result
}

export { get, GetOptions, GetResult }
