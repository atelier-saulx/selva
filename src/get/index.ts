import { Id } from '../schema'
// import { GetItem, GetResult, GetOptions } from './types'
import { SelvaClient } from '..'
import getField from './getField'
import { setNestedResult } from './nestedFields'
import inherit from './inherit'

// re-add id

export type Inherit =
  | boolean
  | {
      $type?: string | string[]
      $name?: string | string[]
      $item?: Id | Id[]
    }

type Find = {
  $traverse?: 'descendants' | 'ancestors' | 'children' | 'parents'
}

type List = {
  $range?: [number, number]
  $find?: Find
}

type GetField<T> = {
  $field?: string | string[]
  $inherit?: Inherit
  $list?: List
  $default?: T
}

// want with $ come on :D
type Item = {
  [key: string]: any
}

// update $language for default + text (algebraic)
export type GetItem<T = Item> = {
  [P in keyof T]?: T[P] extends Item[]
    ? GetItem<T>[] | true
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
  $language?: string
}

// tmp be able to return anything
// this is the result
// we can also make something else e.g. GetApi (All), Get (Item)
type GetResult = {
  [key: string]: any
}

export async function getInner(
  client: SelvaClient,
  props: GetItem,
  result: GetResult,
  id: Id,
  field?: string,
  language?: string,
  version?: string,
  ignore?: '$' | '$inherit' | '$list' | '$find' | '$filter' // when from inherit
): Promise<boolean> {
  let isComplete = true
  let hasKeys = false
  for (const key in props) {
    if (key[0] !== '$') {
      hasKeys = true
      const f = field ? field + '.' + key : key
      if (props[key] === true) {
        if (!(await getField(client, id, f, result, language, version))) {
          isComplete = false
        }
      } else {
        if (
          !(await getInner(
            client,
            props[key],
            result,
            id,
            f,
            language,
            version
          ))
        ) {
          isComplete = false
        }
      }
    }
  }

  // make no inherit a field - ignore field
  //
  if (
    (!ignore || (ignore !== '$' && ignore !== '$inherit')) &&
    props.$inherit &&
    (!isComplete || !hasKeys)
  ) {
    if (!hasKeys) {
      const complete = await getField(
        client,
        id,
        field,
        result,
        language,
        version
      )
      if (!complete) {
        await inherit(client, id, field || '', props, result, language, version)
      }
    } else {
      await inherit(client, id, field || '', props, result, language, version)
    }
  }

  if (props.$default) {
    const complete = await getField(
      client,
      id,
      field,
      result,
      language,
      version
    )
    if (!complete) {
      setNestedResult(result, field, props.$default)
    }
  }

  return isComplete
}

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  // const result: GetResult = {}
  // const { $version: version, $id: id, $language: language } = props
  // if (id) {
  //   await getInner(client, props, result, id, undefined, language, version)
  // } else {
  //   // only find
  // }
  // return result
  const getResult = await client.fetch(props)
  return getResult
}

export { get, GetResult }
