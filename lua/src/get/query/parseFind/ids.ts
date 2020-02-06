import * as redis from '../../../redis'
import { getPrefixFromType } from '../../../typeIdMapping'
import { isArray, stringStartsWith } from '../../../util'
import { Filter } from '~selva/get/types'
import { Fork } from '../types'
import parseFilters from '../parseFilters'

const parseTypeFilter = (filter: Filter, ids: string[]): string[] => {
  const r: string[] = []
  if (isArray(filter.$value)) {
    const v: string[] = []
    for (let i = 0; i < ids.length; i++) {
      for (let j = 0; j < filter.$value.length; j++) {
        if (!v[j]) {
          v[j] = getPrefixFromType(<string>filter.$value[j])
        }
        if (stringStartsWith(ids[i], v[j])) {
          r[r.length] = ids[i]
          break
        }
      }
    }
  } else {
    const v = getPrefixFromType(<string>filter.$value)
    for (let i = 0; i < ids.length; i++) {
      if (stringStartsWith(ids[i], v)) {
        r[r.length] = ids[i]
      }
    }
  }
  return r
}

function parseIds(
  filters: Filter[],
  ids: string[],
  needsQeury?: boolean
): [Fork | string[], string | null] {
  if (filters.length !== 0) {
    let typeFilter: Filter | undefined
    for (let i = 0; i < filters.length; i++) {
      if (filters[i].$field === 'type') {
        typeFilter = filters[i]
        table.remove(filters, i + 1)
        break
      }
    }
    if (typeFilter) {
      ids = parseTypeFilter(typeFilter, ids)
    }
    if (filters.length === 0 && !needsQeury) {
      // empty first arg to get unified response with redisSearch
      table.insert(ids, 1, '')
      return [ids, null]
    } else {
      filters[filters.length] = {
        $field: 'id',
        $value: ids,
        $operator: '='
      }
      return parseFilters(filters)
    }
  } else {
    // empty first arg to get unified response with redisSearc
    table.insert(ids, 1, '')
    return [ids, null]
  }
}

export default parseIds
