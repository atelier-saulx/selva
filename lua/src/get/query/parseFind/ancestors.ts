import * as redis from '../../../redis'
import { getPrefixFromType } from '../../../typeIdMapping'
import { isArray, stringStartsWith } from '../../../util'
import { Filter } from '~selva/get/types'
import { Fork } from '../types'
import parseFilters from '../parseFilters'

// first type parser

function parseFindAncestors(
  filters: Filter[],
  id: string
): [Fork | string[], string | null] {
  const ancestors = redis.zrange(id + '.ancestors')
  if (filters.length !== 0) {
    if (filters.length === 1 && filters[0].$field === 'type') {
      const filter = filters[0]
      const r: string[] = ['']
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
      return [r, null]
    } else {
      // FIXME: check if type field - reuse same logic
      const a: string[] = []
      for (let i = 0; i < ancestors.length; i++) {
        a[a.length] = ancestors[i]
      }
      filters[filters.length] = {
        $field: 'ancestors',
        $value: a,
        $operator: '='
      }
      const r = parseFilters(filters)
      return r
    }
  } else {
    table.insert(ancestors, 1, '')
    return [ancestors, null]
  }
}

export default parseFindAncestors
