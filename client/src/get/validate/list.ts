import { List, GetOptions } from '../types'
import { SelvaClient } from '../..'

import validateInherit from './inherit'

import validateFind from './find'
import validateSort from './sort'

export default function validateList(
  parentProp: GetOptions,
  client: SelvaClient,
  list: List,
  path: string
): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $list'
    }

    throw new Error(
      `${mainMsg} for ${path}.$list. Required type boolean or object with any of the following properties:
        {
          $offset: number (optional)
          $limit: number (optional)
          $sort: { $field: string, $order?: 'asc' | 'desc' } or array of these sort objects (optional)
          $find: FindOptions (optional) -- see below
          $inherit: InheritOptions (optional) -- see below            
        }

        FindOptions:
          {
            $traverse: 'descendants' | 'ancestors' | string | string[] (optional)
            $filter: Filter | FilterOptions[] (and by default) (optional)
            $find: FindOptions (recursive find to find within the results) (optional) 
          }

        FilterOptions:
          {
            $operator: '=' | '!=' | '>' | '<' | '..'
            $field: string
            $value: string | number | (string | number)[]
            $and: FilterOptions (adds an additional condition) (optional)
            $or: FilterOptions (adds optional condition) (optional)
          }

        // TODO: put these in an object so they don't have to be copied
        InheritOptions:
        true
        or
        {
          $item: string | string[] (type)
          $required: string | string[] (field name) (optional)
        } 
        or
        {
          $type: string | string[] or $name?: string | string[] but not both (optional)
          $merge: boolean (optional)
        }
    `
    )
  }

  if (typeof list === 'boolean') {
    return
  }

  if (typeof list === 'object') {
    for (const field in list) {
      if (!field.startsWith('$')) {
        err(
          `Only operators starting with $ are allowed in $list, ${field} not allowed`
        )
      } else if (field === '$offset') {
        if (typeof list.$offset !== 'number') {
          err(`$offset has to be an number, ${list.$offset} specified`)
        }
      } else if (field === '$limit') {
        if (typeof list.$limit !== 'number') {
          err(`$limit has to be an number, ${list.$limit} specified`)
        }
      } else if (field === '$sort') {
        if (Array.isArray(list.$sort)) {
          for (const sort of list.$sort) {
            validateSort(client, sort, path + '.$list')
          }
        } else {
          validateSort(client, list.$sort, path + '.$list')
        }
      } else if (field === '$find') {
        validateFind(parentProp, client, list.$find, path + '.$list')
      } else if (field === '$inherit') {
        validateInherit(client, list.$inherit, path + '.$list')
      } else {
        err(`Operator ${field} not allowed`)
      }
    }

    return
  }

  err()
}
