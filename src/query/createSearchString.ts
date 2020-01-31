import { GetSchemaResult } from '~selva/schema/getSchema'

// need to check the schema and operator you are using
// can also throw for things that are not possible
// (@x:foo)|(@y:bar)
// double the fields
// https://oss.redislabs.com/redisearch/Query_Syntax.html
// FT.SEARCH cars "@country:korea @engine:(diesel|hybrid) @class:suv"
// FT.EXPLAIN {index} {query}
// @ancestors: [] (@y:flap|@x:bar)

const transformValue = (filter, fn) => {
  if (Array.isArray(filter.$value)) {
    for (let i = 0; i < filter.$value.length; i++) {
      filter.$value[i] = fn(filter.$value[i])
    }
  } else {
    filter.$value = fn(filter.$value)
  }
}

const transformOperator = (filter, schema: GetSchemaResult) => {
  // depends on tye
  if (filter.$operator === '!=') {
    transformValue(filter, (v: string | number) => `${v}`)
    return '|'
  } else if (filter.$operator === '=') {
    transformValue(filter, (v: string | number) => `${v}`)
    return '|'
  }
}

const addField = (filter, schema: GetSchemaResult): string => {
  if (filter.$operator === '!=') {
    return `(-(@${filter.$field}:{${filter.$value}}))`
  } else {
    return `(@${filter.$field}:{${filter.$value}})`
  }
}

const createSearchString = (filters, schema: GetSchemaResult) => {
  const searchString = []
  if (filters.$and && filters.$or) {
    throw new Error('cannot have $or and $and on one intermediate result level')
  }

  // need {} everywhere where its a tag!

  if (filters.$and) {
    for (let filter of filters.$and) {
      if (!filter.$or) {
        const seperator = transformOperator(filter, schema)
        if (Array.isArray(filter.$value)) {
          filter.$value = `${filter.$value.join(seperator)}`
        }
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
          const seperator = transformOperator(filter, schema)
          if (Array.isArray(filter.$value)) {
            filter.$value = `${filter.$value.join(seperator)}`
          }
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
