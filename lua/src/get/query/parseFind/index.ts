import parseFilters from '../parseFilters'
import { Fork } from '../types'
import { isArray } from '../../../util'
import { Find, Filter } from '~selva/get/types'
import parseFindIds from './ids'
import * as redis from '../../../redis'
import * as logger from '../../../logger'
import get from '../../index'

function parseFind(
  opts: Find,
  id: string,
  needsQeury?: boolean
): [Fork | string[], string | null] {
  let { $traverse, $filter: filterRaw } = opts
  if (!filterRaw) {
    filterRaw = opts.$filter = []
  }
  if (!isArray(filterRaw)) {
    filterRaw = opts.$filter = [filterRaw]
  }
  const filters: Filter[] = filterRaw
  if ($traverse) {
    if ($traverse === 'descendants') {
      if (filters) {
        filters[filters.length] = {
          $field: 'ancestors',
          $value: id,
          $operator: '='
        }
        return parseFilters(filters)
      } else {
        // really heavy operation - also weird location...
        const { descendants } = get({ $id: id, descendants: true })
        table.insert(descendants, 1, '')
        return [descendants, null]
      }
    } else if ($traverse === 'ancestors') {
      const ancestors = redis.zrange(id + '.ancestors')
      return parseFindIds(filters, ancestors, needsQeury)
    } else {
      logger.info(id + '.' + $traverse)
      const ids = redis.smembers(id + '.' + $traverse)
      logger.info(ids)
      return parseFindIds(filters, ids, needsQeury)
    }
  } else {
    return [{ isFork: true }, 'Need to allways define $traverse for now']
  }
}

export default parseFind
