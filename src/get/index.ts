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

type GetField<T> = {
  $field?: Field
  $inherit?: Inherit
  $list?: List
  $default?: T
}

type GetItem<T = Item> = {
  [P in keyof T]?: T[P] extends Item[]
    ? GetItem<T>[] | true
    : T[P] extends object
    ? (GetItem<T[P]> & GetField<T>) | true
    : T[P] extends number
    ? GetField<T[P]> | true
    : T[P] extends string
    ? GetField<T[P]> | true
    : T[P] extends boolean
    ? GetField<T[P]> | true
    : GetField<T[P]> | true
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

  // sad :( ah wait also need union on get item itself ?
  if (props.$default) {
    console.log('ok')
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
