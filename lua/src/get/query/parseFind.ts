import { Schema } from '~selva/schema/index'
import parseFilters from './parseFilters'
import { QeuryResult } from './types'
import { isArray } from '../../util'
import { Find, Filter } from '~selva/get/types'

function parseFind(
  result: QeuryResult,
  opts: Find,
  id: string,
  schema: Schema,
  field?: string
): string | null {
  let { $traverse, $filter: filterRaw, $find } = opts
  if (!filterRaw) {
    filterRaw = opts.$filter = []
  }
  if (!isArray(filterRaw)) {
    filterRaw = opts.$filter = [filterRaw]
  }

  let $filter: Filter[] = filterRaw

  if ($traverse) {
    if ($traverse === 'descendants') {
      if ($filter) {
        $filter[$filter.length] = {
          $field: 'ancestors',
          $value: id,
          $operator: '='
        }
        const [_, err] = parseFilters(result, $filter, schema)
        if (err) {
          return err
        }
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
      const err = parseFind(result, $find, id, schema, field)
      if (err) {
        return err
      }
    }
  } else {
    return 'Need to allways define $traverse for now'
  }
  return null
}

export default parseFind
