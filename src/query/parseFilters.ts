import compareFilters from './compareFilters'
import addResult from './addResult'
import { GetSchemaResult } from '~selva/schema/getSchema'

const reduceFilter = (filter, $filter) => {
  // reduces $and statement
  if (filter.$and && !filter.$or) {
    $filter.push(filter.$and)
    delete filter.$and
  }
}

// reduce the filter to an easier to parse intermediate format
const parseFilters = (result, $filter, schema: GetSchemaResult) => {
  for (let i = 0; i < $filter.length; i++) {
    let filter = $filter[i]
    const search =
      schema.searchIndexes.default &&
      schema.searchIndexes.default[filter.$field]

    if (search) {
      filter.$search = search
      const operator = filter.$operator
      if (
        search[0] !== 'NUMERIC' &&
        (operator === '>' ||
          operator === '<' ||
          operator === '<=' ||
          operator === '..' ||
          operator === '>=')
      ) {
        throw new Error(
          `Cannot have numeric comparisons on other search types then NUMERIC ${filter.$field}`
        )
      }
    } else {
      throw new Error(
        `Cannot search fields that are not indexed ${filter.$field}`
      )
    }

    if (filter.$search[0] === 'NUMERIC') {
      if (Array.isArray(filter.$value)) {
        for (let i = 0; i < filter.$value.length; i++) {
          if (filter.$value[i] === 'now') {
            filter.$value[i] = Date.now()
          }
        }
      }
      if (filter.$value === 'now') {
        filter.$value = Date.now()
      }
    }

    reduceFilter(filter, $filter)

    filter = compareFilters(result, filter, schema)

    if (filter) {
      if (filter.$or) {
        const or = parseFilters(
          { filters: {}, reverseMap: {} },
          [filter.$or],
          schema
        )
        let r
        if (or.filters.$and.length === 1) {
          r = { $or: [filter, or.filters.$and[0]] }
        } else {
          r = { $or: [filter, or.filters] }
        }
        delete filter.$or
        if (filter.$and) {
          const and = parseFilters(
            { filters: { $and: [filter] }, reverseMap: {} },
            [filter.$and],
            schema
          )
          delete filter.$and
          filter = and
          r.$or[0] = and.filters
        }
        for (let i = 0; i < r.$or.length; i++) {
          const f = r.$or[i]
          if (f.$or) {
            if (f.$and) {
              f.$or = [{ $and: r.$and }, ...f.$or]
            }
          }
        }

        // this is different
        addResult(result, r, '$and')
      } else {
        addResult(result, filter, '$and')
      }
    }
  }
  return result
}

export default parseFilters
