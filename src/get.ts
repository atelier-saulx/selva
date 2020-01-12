import { Item, Id, Language, Type, Text, Field } from './schema'
import { SelvaClient } from './'

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
}

// tmp be able to return anythign
// we can also make something else
type GetResult<T = Item> = {
  [P in keyof T]?: true | T[P] extends Item[]
    ? GetResult<T>[]
    : T[P] extends Text
    ? T[P] | string
    : T[P] extends object
    ? GetResult<T[P]>
    : T[P]
} & { [key: string]: any }

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const result: Item = {}
  if (props.$id) {
    const id = props.$id
    // all actions
    let keys: string[]
    for (let key in props) {
      if (key[0] !== '$') {
        // need to generate this fro the type -- to double
        if (props[key] === true) {
          // load keys if you need to load all fields of nested ones
          if (
            key === 'title' ||
            key === 'auth' ||
            key === 'image' ||
            key === 'video' ||
            key === 'auth'
          ) {
            if (!keys) {
              keys = await client.redis.hkeys(id)
            }
            const fieldResult = {}
            for (const field of keys) {
              const fields = field.split('.')
              if (fields[0] === key) {
                const val = await client.redis.hget(id, field)
                // fieldResult
                if (fields.length > 2) {
                  console.log('DEEP GO')
                } else {
                  fieldResult[fields[1]] = val
                }
              }
            }
            result[key] = fieldResult
          } else if (key === 'children' || key === 'parents') {
            // smurky
          } else if (key === 'ancestors') {
            // blarf
          } else if (key === 'descendants') {
            // return
          }
          // else if (key === 'type') {
          // store type as 2 letter combination
          //}
          else if (key === 'id') {
            // id ----
            result.id = id
          } else {
            // need to cast types
            const val = await client.redis.hget(id, key)
            // would be nice to generate this from type
            if (
              key === 'value' ||
              key === 'age' ||
              key === 'status' ||
              key === 'date' ||
              key === 'start' ||
              key === 'end'
            ) {
              result[key] = val * 1
            } else {
              result[key] = val
            }
          }
        }
      }
    }
  } else {
    // only find
  }
  return result
}

export { get, GetOptions, GetResult }
