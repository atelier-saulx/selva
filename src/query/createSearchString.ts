const createSearchString = (filters, schema) => {
  const searchString = []
  if (filters.$and && filters.$or) {
    throw new Error('cannot have $or and $and on one intermediate result level')
  }

  const transformValue = (filter, fn) => {
    if (Array.isArray(filter.$value)) {
      for (let i = 0; i < filter.$value.length; i++) {
        filter.$value[i] = fn(filter.$value[i])
      }
    } else {
      filter.$value = fn(filter.$value)
    }
  }

  const transformOperator = filter => {
    if (filter.$operator === '!=') {
      transformValue(filter, v => `-"${v}"`)
      return ' '
    } else if (filter.$operator === '=') {
      transformValue(filter, v => `"${v}"`)
      return '|'
    }
  }

  // need to check the schema and operator you are using
  // can also throw for things that are not possible
  // (@x:foo)|(@y:bar)
  // double the fields
  // https://oss.redislabs.com/redisearch/Query_Syntax.html
  // FT.SEARCH cars "@country:korea @engine:(diesel|hybrid) @class:suv"
  // FT.EXPLAIN {index} {query}
  // @ancestors: [] (@y:flap|@x:bar)

  if (filters.$and) {
    for (let filter of filters.$and) {
      if (!filter.$or) {
        const seperator = transformOperator(filter)
        if (Array.isArray(filter.$value)) {
          filter.$value = `{${filter.$value.join(seperator)}}`
        }
        searchString.push(`@${filter.$field}:${filter.$value}`)
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
          const seperator = transformOperator(filter)
          if (Array.isArray(filter.$value)) {
            filter.$value = `{${filter.$value.join(seperator)}}`
          }
          searchString.push(`@${filter.$field}:${filter.$value}`)
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
