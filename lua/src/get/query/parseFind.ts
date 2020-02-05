import { Schema } from '~selva/schema/index'
import parseFilters from './parseFilters'
import { Fork } from './types'
import { isArray } from '../../util'
import { Find, Filter } from '~selva/get/types'

function parseFind(
  opts: Find,
  id: string,
  field?: string
): [Fork, string | null] {
  let { $traverse, $filter: filterRaw, $find } = opts
  if (!filterRaw) {
    filterRaw = opts.$filter = []
  }
  if (!isArray(filterRaw)) {
    filterRaw = opts.$filter = [filterRaw]
  }
  const $filter: Filter[] = filterRaw
  if ($traverse) {
    if ($traverse === 'descendants') {
      if ($filter) {
        $filter[$filter.length] = {
          $field: 'ancestors',
          $value: id,
          $operator: '='
        }
        return parseFilters($filter)
      }
    } else if ($traverse === 'ancestors') {
      if ($filter) {
        // if id or type can do something smart - else nested query on the results
      } else {
        // just return the ancestors
      }
    } else if ($traverse === 'children') {
      // easier
    } else if ($traverse === 'parents') {
      // easier
    }
    if ($find) {
      return parseFind($find, id, field)
    }
  } else {
    return [{ isFork: true }, 'Need to allways define $traverse for now']
  }
  return [{ isFork: true }, 'No valid options in find to parse']
}

export default parseFind
