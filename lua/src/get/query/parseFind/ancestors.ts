import * as redis from '../../../redis'
import { getPrefixFromType } from '../../../typeIdMapping'
import { isArray, stringStartsWith } from '../../../util'
import { Filter } from '~selva/get/types'
import { Fork } from '../types'
import parseFilters from '../parseFilters'

const parseTypeFilter = (filter: Filter, ancestors: string[]): string[] => {
  const r: string[] = []
  if (isArray(filter.$value)) {
    const v: string[] = []
    for (let i = 0; i < ancestors.length; i++) {
      for (let j = 0; j < filter.$value.length; j++) {
        if (!v[j]) {
          v[j] = getPrefixFromType(<string>filter.$value[j])
        }
        if (stringStartsWith(ancestors[i], v[j])) {
          r[r.length] = ancestors[i]
          break
        }
      }
    }
  } else {
    const v = getPrefixFromType(<string>filter.$value)
    for (let i = 0; i < ancestors.length; i++) {
      if (stringStartsWith(ancestors[i], v)) {
        r[r.length] = ancestors[i]
      }
    }
  }
  return r
}

function parseFindAncestors(
  filters: Filter[],
  id: string
): [Fork | string[], string | null] {
  let ancestors = redis.zrange(id + '.ancestors')
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
      ancestors = parseTypeFilter(typeFilter, ancestors)
    }
    if (filters.length === 0) {
      // empty first arg to get unified response with redisSearch
      table.insert(ancestors, 1, '')
      return [ancestors, null]
    } else {
      filters[filters.length] = {
        $field: 'id',
        $value: ancestors,
        $operator: '='
      }
      return parseFilters(filters)
    }
  } else {
    // empty first arg to get unified response with redisSearc
    table.insert(ancestors, 1, '')
    return [ancestors, null]
  }
}

export default parseFindAncestors
