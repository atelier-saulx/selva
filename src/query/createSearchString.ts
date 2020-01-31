const createSearchString = (filters, schema) => {
  const searchString = []
  if (filters.$and && filters.$or) {
    throw new Error('cannot have $or and $and')
  }

  if (filters.$and) {
    for (let filter of filters.$and) {
      if (!filter.$or) {
        if (Array.isArray(filter.$value)) {
          filter.$value = `{${filter.$value.join('|')}}`
        }
        searchString.push(`@${filter.$field}:${filter.$value}`)
      } else {
        const nestedSearch = createSearchString(filter, schema)
        //   console.log('OR --->', nestedSearch)
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
          if (Array.isArray(filter.$value)) {
            filter.$value = `{${filter.$value.join('|')}}`
          }
          searchString.push(`@${filter.$field}:${filter.$value}`)
        }
      } else {
        const nestedSearch = createSearchString(filter, schema)
        //   console.log('AND --->', nestedSearch)
        searchString.push(nestedSearch)
      }
    }
    return `(${searchString.join('|')})`
  }
}

export default createSearchString
