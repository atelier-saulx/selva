import parseFilters from './parseFilters'
import { Fork } from './types'
import { isArray, stringStartsWith } from '../../util'
import { Find, Filter } from '~selva/get/types'
import * as logger from '../../logger'
import { getSchema } from '../../schema/index'
import * as redis from '../../redis'
import { getPrefixFromType } from '../../typeIdMapping'

function parseFind(
  opts: Find,
  id: string,
  field?: string
): [Fork | string[], string | null] {
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
      } else {
        // return all descendants
        logger.info('return all desc')
      }
    } else if ($traverse === 'ancestors') {
      if ($filter.length) {
        if ($filter.length === 1 && $filter[0].$field === 'type') {
          const filter = $filter[0]
          const ancestors = redis.zrange(id + '.ancestors')
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
          const ancestors = redis.zrange(id + '.ancestors')
          const r: string[] = ['']
          for (let i = 0; i < ancestors.length; i++) {
            r[r.length] = ancestors[i]
          }
          return [r, null]
        }
        // if id or type can do something smart - else nested query on the results
      } else {
        const ancestors = redis.zrange(id + '.ancestors')
        table.insert(ancestors, 1, '')
        return [ancestors, null]
      }
      // means we can use any field here that is a list
    } else if ($traverse === 'children') {
      // easier just make this use another set of functions
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
