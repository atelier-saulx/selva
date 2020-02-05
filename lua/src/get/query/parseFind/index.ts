import parseFilters from '../parseFilters'
import { Fork } from '../types'
import { isArray } from '../../../util'
import { Find, Filter } from '~selva/get/types'
import * as logger from '../../../logger'
import parseFindAncestors from './ancestors'

function parseFind(opts: Find, id: string): [Fork | string[], string | null] {
  let { $traverse, $filter: filterRaw, $find } = opts
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
        // return all descendants
        logger.info('return all desc')
      }
    } else if ($traverse === 'ancestors') {
      return parseFindAncestors(filters, id)
    } else if ($traverse === 'children') {
      // easier just make this use another set of functions
    } else if ($traverse === 'parents') {
      // easier
    }
    if ($find) {
      return parseFind($find, id)
    }
  } else {
    return [{ isFork: true }, 'Need to allways define $traverse for now']
  }
  return [{ isFork: true }, 'No valid options in find to parse']
}

export default parseFind
