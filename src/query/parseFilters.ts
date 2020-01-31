import compareFilters from './compareFilters'
import addResult from './addResult'

const reduceFilter = (filter, $filter) => {
  // reduces $and statement
  if (filter.$and && !filter.$or) {
    $filter.push(filter.$and)
    delete filter.$and
  }
}

// reduce the filter to an easier to parse intermediate format
const parseFilters = (result, $filter, schema) => {
  for (let i = 0; i < $filter.length; i++) {
    let filter = $filter[i]
    reduceFilter(filter, $filter)
    filter = compareFilters(result, filter, schema)
    if (filter) {
      if (filter.$or) {
        const or = parseFilters(
          { filters: { $and: [], $or: [] }, reverseMap: {} },
          [filter.$or],
          schema
        )
        let r
        if (or.filters.$and.length === 1) {
          r = { $or: [filter, or.filters.$and[0]], $and: [] }
        } else {
          r = { $or: [filter, or.filters], $and: [] }
        }
        delete filter.$or
        if (filter.$and) {
          const and = parseFilters(
            { filters: { $and: [filter], $or: [] }, reverseMap: {} },
            [filter.$and],
            schema
          )
          delete filter.$and
          filter = and
          r.$or[0] = and.filters
        }
        if (r.$and.length === 0) {
          delete r.$and
        }
        for (let i = 0; i < r.$or.length; i++) {
          const f = r.$or[i]
          if (f.$or) {
            if (f.$or.length === 0) {
              delete f.$or
            } else {
              if (f.$and) {
                f.$or = [{ $and: r.$and }, ...f.$or]
              }
            }
          }
        }
        addResult(result, r, '$and')
      } else {
        addResult(result, filter, '$and')
      }
    }
  }
  return result
}

export default parseFilters
