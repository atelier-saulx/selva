const filterExists = (a, b) => {
  return (
    !a.$or &&
    a.$operator === b.$operator &&
    a.$field === b.$field &&
    a.$value === b.$value
  )
}

// can create fun checks now
const compareFilters = (result, filter, schema) => {
  const a = result.reverseMap[filter.$field]
  if (!a) {
    result.reverseMap[filter.$field] = []
    return filter
  }

  if (filter.$or) {
    return filter
  }

  for (let i = 0; i < a.length; i++) {
    // make something nice for this block
    const prevFilter = a[i]

    if (filterExists(prevFilter, filter)) {
      return false
    }

    if (
      prevFilter.$operator === '=' &&
      filter.$operator === '=' &&
      !prevFilter.$value !== filter.$value &&
      typeof prevFilter.$value !== typeof filter.$value
    ) {
      if (Array.isArray(prevFilter.$value)) {
        if (prevFilter.$value.indexOf(filter.$value) !== -1) {
          // exists fine
          return false
        }
      } else if (Array.isArray(filter.$value)) {
        if (filter.$value.indexOf(prevFilter.$value) !== -1) {
          prevFilter.$value = filter.$value
          return false
        }
      }
    } else if (prevFilter.$operator === '=' && filter.$operator === '=') {
      // if schema is set && tag is the real check
      if (filter.$field === 'ancestors') {
        if (!Array.isArray(prevFilter.$value)) {
          prevFilter.$value = [prevFilter.$value]
        }
        prevFilter.$value.push(filter.$value)
        return false
      }
    } else if (prevFilter.$operator === '>' && filter.$operator === '>') {
      // make this nice
    }
  }
  return filter
}

export default compareFilters
