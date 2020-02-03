import { GetSchemaResult } from '~selva/schema/getSchema'

// need to check the schema and operator you are using
// can also throw for things that are not possible
// (@x:foo)|(@y:bar)
// double the fields
// https://oss.redislabs.com/redisearch/Query_Syntax.html
// FT.SEARCH cars "@country:korea @engine:(diesel|hybrid) @class:suv"
// FT.EXPLAIN {index} {query}
// @ancestors: [] (@y:flap|@x:bar)

// const transformValue = (filter, fn) => {
//   if (Array.isArray(filter.$value)) {
//     for (let i = 0; i < filter.$value.length; i++) {
//       filter.$value[i] = fn(filter.$value[i])
//     }
//   } else {
//     filter.$value = fn(filter.$value)
//   }
// }

// const transformOperator = (filter, schema: GetSchemaResult) => {
//   // depends on field type
//   if (filter.$operator === '!=') {
//     // transformValue(filter, (v: string | number) => `${v}`)
//     return '|'
//   } else if (filter.$operator === '=') {
//     // transformValue(filter, (v: string | number) => `${v}`)
//     return '|'
//   }
// }

const returnNumber = (filter, value) => {
  if (filter.$operator === '>') {
    // depending on schema...
    return `(@${filter.$field}:[${value},inf])`
  } else if (filter.$operator === '..') {
    return `(@${filter.$field}:[${value[0]},${value[1]}])`
  } else if (filter.$operator === '!=') {
    return `(-(@${filter.$field}:[${value},${value}]))`
  } else if (filter.$operator === '=') {
    return `(@${filter.$field}:[${value},${value}])`
  }
}

// ADD TEXT AND GEO
// SEARCH DB
// LEVEN STEIN DISTANCE index language
// SEARCH PREFIXES
// also incolude language in searching REAL SEARCH

const addField = (filter, schema: GetSchemaResult): string => {
  // depends on field type
  const type = filter.$search[0]
  const operator = filter.$operator
  if (type === 'TAG') {
    if (Array.isArray(filter.$value)) {
      filter.$value = `${filter.$value.join('|')}`
    }
    if (operator === '!=') {
      return `(-(@${filter.$field}:{${filter.$value}}))`
    } else if (operator === '=') {
      return `(@${filter.$field}:{${filter.$value}})`
    }
  } else if (type === 'NUMERIC') {
    if (Array.isArray(filter.$value) && filter.$operator !== '..') {
      return (
        '(' + filter.$value.map(v => returnNumber(filter, v)).join('|') + ')'
      )
    } else {
      return returnNumber(filter, filter.$value)
    }
  } else if (type === 'TEXT') {
    // DO THINGS
    // INCLUDE LANGUAGE ETC
  } else if (type === 'GEO') {
  }
}

const createSearchString = (filters, schema: GetSchemaResult) => {
  const searchString = []
  if (filters.$and && filters.$or) {
    throw new Error('cannot have $or and $and on one intermediate result level')
  }
  if (filters.$and) {
    for (let filter of filters.$and) {
      if (!filter.$or) {
        searchString.push(addField(filter, schema))
      } else {
        const nestedSearch = createSearchString(filter, schema)
        searchString.push(nestedSearch)
      }
    }
    return `(${searchString.join(' ')})`
  } else if (filters.$or) {
    for (let filter of filters.$or) {
      if (!filter.$and) {
        if (filter.$or) {
          const nestedSearch = createSearchString(filter, schema)
          searchString.push(nestedSearch)
        } else {
          searchString.push(addField(filter, schema))
        }
      } else {
        const nestedSearch = createSearchString(filter, schema)
        searchString.push(nestedSearch)
      }
    }
    return `(${searchString.join('|')})`
  }
}

export default createSearchString
