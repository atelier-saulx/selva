import { Find } from '../types'
import { SelvaClient } from '../..'

import checkAllowed from './checkAllowed'
import validateFilter from './filter'

export default function validateFind(
  client: SelvaClient,
  find: Find,
  path: string,
  findInFind: boolean = false
): void {
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

  if (find.$db && !findInFind) {
    err(
      `Unupported field $db in ${find}, $find.$db supported only when finds are directly nested: $find.$find.$db`
    )
  }

  if (find.$traverse) {
    if (typeof find.$traverse !== 'string' && !Array.isArray(find.$traverse)) {
      err(`Unupported type for $traverse ${find.$traverse}`)
    }
  }

  if (find.$find) {
    validateFind(client, find.$find, path + '.$find', true)
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
