import parseFilters from '../parseFilters'
import { Fork } from '../types'
import { isArray, ensureArray } from '../../../util'
import { Find, Filter } from '~selva/get/types'
import parseFindIds from './ids'
import * as redis from '../../../redis'
import * as logger from '../../../logger'

const getIds = (traverse: string, ids: string[]): string[] => {
  if (traverse === 'ancestors') {
    if (ids.length === 1) {
      return redis.zrange(ids[0] + '.ancestors')
    } else {
      const rMap: Record<string, true> = {}
      const ancestors: string[] = []
      for (let i = 0; i < ids.length; i++) {
        const a: string[] = redis.zrange(ids[i] + '.ancestors')
        for (let j = 0; j < a.length; j++) {
          if (!rMap[a[j]]) {
            ancestors[ancestors.length] = a[j]
            rMap[a[j]] = true
          }
        }
      }
      return ancestors
    }
  } else {
    if (ids.length === 1) {
      return redis.smembers(ids[0] + '.' + traverse)
    } else {
      const fields: string[] = []
      for (let i = 0; i < ids.length; i++) {
        fields[fields.length] = ids[i] + '.' + traverse
      }
      return redis.sunion(fields)
    }
  }
}

// pass meta along
function parseFind(
  opts: Find & { $fields?: string[] },
  ids: string[]
): [Fork | string[], string | null] {
  let { $traverse, $filter: filterRaw, $fields } = opts
  if (!filterRaw) {
    filterRaw = opts.$filter = []
  }
  if (!isArray(filterRaw)) {
    filterRaw = opts.$filter = [filterRaw]
  }
  const filters: Filter[] = filterRaw

  // if $traverse is an array use that array
  if ($traverse) {
    if ($traverse === 'descendants') {
      if (filters) {
        filters[filters.length] = {
          $field: 'ancestors',
          $value: ids, // means an or
          $operator: '='
        }
        return parseFilters(filters)
      } else {
        // if (ids.length > 1) {
        return [[], 'Descendants without a filter cannot have multiple ids yet']
        // }
        // const { descendants } = get({ $id: ids[0], descendants: true })
        // table.insert(descendants, 1, '')
        // return [descendants, null]
      }
    } else if ($traverse === 'ancestors') {
      // for loop here
      const ancestors = getIds($traverse, ids)
      return parseFindIds(filters, ancestors)
    } else if (isArray($traverse)) {
      // short hand to do iteration over multiple ids
      return parseFindIds(filters, $traverse)
    } else {
      const resultIds = getIds($traverse, ids)
      return parseFindIds(filters, resultIds)
    }
  } else if ($fields) {
    let resultIds: string[] = []
    for (const field of $fields) {
      let res = getIds(field, ids)
      if (res && res.length > 0) {
        resultIds = res
        break
      }
    }

    return parseFindIds(filters, resultIds)
  } else {
    return [{ isFork: true }, 'Need to allways define $traverse for now']
  }
}

export default parseFind
