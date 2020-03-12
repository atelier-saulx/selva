import { getPrefixFromType } from '../../../typeIdMapping'
import { isArray, stringStartsWith } from '../../../util'
import { Filter } from '~selva/get/types'
import { Fork, Meta } from '../types'
import parseFilters from '../parseFilters'
import * as logger from '../../../logger'

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
  meta: Meta
): [Fork | string[], string | null] {
  if (filters.length !== 0) {
    let typeFilter: Filter | undefined
    for (let i = 0; i < filters.length; i++) {
      // add this to meta!
      if (filters[i].$field === 'type') {
        typeFilter = filters[i]
        const oldFilters = filters
        filters = []
        for (let j = 0; j < oldFilters.length; j++) {
          if (j !== i) {
            filters[filters.length] = oldFilters[j]
          }
        }
        break
      }
    }
    if (typeFilter) {
      logger.info('????', meta)

      logger.info('!!!', typeFilter)
      ids = parseTypeFilter(typeFilter, ids)
      logger.info('!!!2')

      if (!meta.type) {
        meta.type = []
      }

      // FIXME: optimize

      logger.info('2????333')

      if (isArray(typeFilter.$value)) {
        for (let j = 0; j < typeFilter.$value.length; j++) {
          meta.type[meta.type.length] = getPrefixFromType(
            <string>typeFilter.$value[j]
          )
        }
      } else {
        meta.type[meta.type.length] = getPrefixFromType(
          <string>typeFilter.$value
        )
      }

      logger.info('2????')
    }
    if (filters.length === 0) {
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
    return [ids, null]
  }
}

export default parseIds
