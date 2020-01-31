const filterExists = (a, b, schema) => {
  // can check for impossible combinations here as well
  if (a.$or) {
    // or is allways seperate
    return false
  }
  const operatorIsEqual = a.$operator === b.$operator
  const fieldIsEqual = a.$field === b.$field
  let valueIsEqual = a.$value === b.$value
  if (operatorIsEqual && fieldIsEqual && valueIsEqual) {
    return true
  }
  return false
}

const compareFilters = (result, filter, schema) => {
  const a = result.reverseMap[filter.$field]
  if (!a) {
    result.reverseMap[filter.$field] = []
    return filter
  }

  if (a.reverseMap) {
    return filter
  }
  // double check if this is ok - means we dont allow arrays for $and, $or
  for (let i = 0; i < a.length; i++) {
    if (filterExists(a[i], filter, schema)) {
      return false
      break
    } else {
      // compare for impossiblities , or merger or changes in types
      // e.g. > , <
      const prevFilter = a[i]
      if (!filter.$or) {
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
        }

        // compare for for example ancestors + set here
        // if ()
        // else we cant compare too hard
      }
    }
  }
  return filter
}

export default compareFilters
