import { GetOptions, GetResult, Sort, List } from '~selva/get/types'
import { hget } from '../../redis'
import { emptyArray, isArray } from '../../util'
import * as logger from '../../logger'
import { Schema } from '../../../../src/schema/index'
import { getSearchIndexes } from '../../schema/index'

function parseList(results: string[], list: List): string[] {
  if (list.$sort) {
    const sort: Sort[] = !isArray(list.$sort) ? [list.$sort] : list.$sort

    const searchIndexes = getSearchIndexes()

    // FIXME: multiple
    const field = sort[0].$field

    const type = searchIndexes.default[field]

    if (!field) {
      logger.info(`${field} is not sortable`)
      return results
    }

    if (type[0] === 'NUMERIC') {
      if (sort[0].$order === 'asc') {
        table.sort(results, (a, b) => {
          const x = hget(a, field)
          const y = hget(b, field)
          return (tonumber(x) || 0) < (tonumber(y) || 0)
        })
      } else {
        table.sort(results, (a, b) => {
          const x = hget(a, field)
          const y = hget(b, field)
          return (tonumber(x) || 0) > (tonumber(y) || 0)
        })
      }
    } else {
      if (sort[0].$order === 'asc') {
        table.sort(results, (a, b) => {
          const x = hget(a, field)
          const y = hget(b, field)
          return (tostring(x) || 0) < (tostring(y) || 0)
        })
      } else {
        table.sort(results, (a, b) => {
          const x = hget(a, field)
          const y = hget(b, field)
          return (tostring(x) || 0) > (tostring(y) || 0)
        })
      }
    }
  }

  if (list.$range) {
    const newResults: string[] = []
    const [start, end] = list.$range
    for (let i = start; i < end && i < results.length; i++) {
      newResults[newResults.length] = results[i]
    }
    return newResults
  }

  return results
}

export default parseList
