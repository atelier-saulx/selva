import parseFilters from '../parseFilters'
import { Fork, Meta } from '../types'
import { isArray } from '../../../util'
import { Find, Filter, Inherit } from '~selva/get/types'
import parseFindIds from './ids'
import * as redis from '../../../redis'
import * as logger from '../../../logger'
import { GetFieldFn } from '../../types'
import { getSchema } from '../../../schema/index'
import { getNestedField } from '../../nestedFields'

const getIds = (
  getField: GetFieldFn,
  traverse: string,
  ids: string[],
  $inherit?: Inherit
): string[] => {
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
      if ($inherit) {
        const intermediateResult = {}
        getField(
          {
            id: true,
            [traverse]: { $inherit }
          },
          getSchema(),
          intermediateResult,
          ids[0],
          ''
        )

        return getNestedField(intermediateResult, traverse)
      }

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
  getField: GetFieldFn,
  opts: Find & { $fields?: string[]; $inherit?: Inherit },
  ids: string[],
  meta: Meta
): [Fork | string[], string | null] {
  let { $traverse, $filter: filterRaw, $fields, $inherit } = opts
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
      const ancestors = getIds(getField, $traverse, ids)
      return parseFindIds(filters, ancestors, meta)
    } else if (isArray($traverse)) {
      // short hand to do iteration over multiple ids
      return parseFindIds(filters, $traverse, meta)
    } else {
      const resultIds = getIds(getField, $traverse, ids, $inherit)
      return parseFindIds(filters, resultIds, meta)
    }
  } else if ($fields) {
    let resultIds: string[] = []
    for (const field of $fields) {
      let res = getIds(getField, field, ids, $inherit)
      if (res && res.length > 0) {
        resultIds = res
        break
      }
    }

    return parseFindIds(filters, resultIds, meta)
  } else {
    return [{ isFork: true }, 'Need to allways define $traverse for now']
  }
}

export default parseFind
