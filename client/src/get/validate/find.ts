import { Find, GetOptions } from '../types'
import { SelvaClient } from '../..'

import checkAllowed from './checkAllowed'
import validateFilter from './filter'

import { get } from '..'
import { addExtraQuery, ExtraQueries } from '.'

export default async function validateFind(
  extraQueries: ExtraQueries,
  parentProp: GetOptions,
  client: SelvaClient,
  find: Find,
  path: string
): Promise<void> {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $find'
    }

    throw new Error(
      `${mainMsg} for ${path}.$find. Required type object with the following properties:
        {
          $traverse: 'descendants' | 'ancestors' | string | string[] (optional)
          $filter: FilterOptions | FilterOptions[] (and by default) (optional)
          $find: FindOptions (find within results of the find) (optional)


        FilterOptions:
          {
            $operator: '=' | '!=' | '>' | '<' | '..'
            $field: string
            $value: string | number | (string | number)[]
            $and: FilterOptions (adds an additional condition) (optional)
            $or: FilterOptions (adds optional condition) (optional)
          }
        `
    )
  }

  const allowed = checkAllowed(
    find,
    new Set(['$traverse', '$filter', '$find', '$db'])
  )
  if (allowed !== true) {
    err(`Unsupported operator or field ${allowed}`)
  }

  if (find.$traverse) {
    const traverse = find.$traverse
    if (typeof traverse === 'object' && !Array.isArray(traverse)) {
      const result = await get(client, {
        $db: traverse.$db,
        $id: traverse.$id,
        traverse: {
          $field: traverse.$field
        }
      })
      const meta = result.$meta
      delete result.$meta

      addExtraQuery(extraQueries, {
        $db: traverse.$db,
        type: 'traverse',
        meta: meta,
        value: result.traverse,
        path: path + '.$find.$traverse'
      })
    } else if (
      typeof find.$traverse !== 'string' &&
      !Array.isArray(find.$traverse)
    ) {
      err(`Unupported type for $traverse ${find.$traverse}`)
    }
  }

  if (find.$find) {
    await validateFind(
      extraQueries,
      parentProp,
      client,
      find.$find,
      path + '.$find'
    )

    if (find.$find.$db) {
      parentProp.__$find = {
        opts: { $value: find.$find },
        ids: { $field: find.$find.$traverse }
      }
      delete find.$find
    }
  }

  if (find.$filter) {
    if (Array.isArray(find.$filter)) {
      for (const filter of find.$filter) {
        validateFilter(client, filter, path + '.$find')
      }
    } else {
      validateFilter(client, find.$filter, path + '.$find')
    }
  }
}
