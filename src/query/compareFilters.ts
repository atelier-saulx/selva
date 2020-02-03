const filterExists = (a, b) => {
  return (
    !a.$or &&
    a.$operator === b.$operator &&
    a.$field === b.$field &&
    a.$value === b.$value
  )
}

const isRelation = (field, schema) => {
  // prob want to add more stuff
  return field === 'ancestors' || field === 'children' || field === 'parents'
}

const isEqual = (a, b, schema) => {
  if (!a.$value !== b.$value && typeof a.$value !== typeof b.$value) {
    if (Array.isArray(a.$value)) {
      if (a.$value.indexOf(b.$value) !== -1) {
        return false
      }
    } else if (Array.isArray(b.$value)) {
      if (b.$value.indexOf(a.$value) !== -1) {
        b.$value = a.$value
        return false
      }
    }
  } else if (isRelation(a.$field, schema)) {
    if (!Array.isArray(b.$value)) {
      b.$value = [b.$value]
    }
    b.$value.push(a.$value)
    return false
  } else if (a.$value !== b.$value) {
    throw new Error(
      `Cannot have 2 isEqual conditions @${a.$field} (${a.$value}) and (${b.$value})`
    )
  }
}

const isLargerThenAndSmallerThen = (a, b, schema) => {
  let $lo, $hi
  if (a.$operator === '>') {
    $lo = a
    $hi = b
  } else {
    $lo = b
    $hi = a
  }
  a.$value = [$lo.$value, $hi.$value]
  a.$operator = '..'
  return false
}

const isLargerThenAndLargerThen = (a, b, schema) => {
  if (b.$value > a.$value) {
    a.$value = b.$value
  }
  return false
}

const isSmallerThenAndSmallerThen = (a, b, schema) => {
  if (b.$value < a.$value) {
    a.$value = b.$value
  }
  return false
}

const isRangeAndLargerOrSmaller = (a, b, schema) => {
  let range = a
  let other = b
  if (b.$operator === '..') {
    range = b
    other = a
  }
  if (other.$operator === '>') {
    if (other.$value > range.$value[1]) {
      throw new Error(
        `Out of bounds range filter ${other.$value} < ${range.$value}`
      )
    }
  }
  if (other.$operator === '<') {
    if (other.$value > range.$value[0]) {
      throw new Error(
        `Out of bounds range filter ${other.$value} > ${range.$value}`
      )
    }
  }
  if (b.$operator === '..') {
    a.$operator = '..'
    a.$value = b.$value
  }
  return false
}

const isNotEqual = (a, b, schema) => {
  if (a.$value !== b.$value) {
    if (!Array.isArray(b.$value)) {
      if (!Array.isArray(a.$value)) {
        a.$value = [a.$value, b.$value]
      } else if (a.$value.indexOf(b.$value) === -1) {
        a.$value.push(b.$value)
      }
    } else {
      if (!Array.isArray(a.$value)) {
        if (a.$value.indexOf(b.$value) !== -1) {
          a.$value = b.$value
        } else {
          b.$value.push(a.$value)
          a.$value = b.$value
        }
      } else {
        for (let i = 0; i < b.$value.length; i++) {
          if (a.$value.indexOf(b.$value[i]) === -1) {
            a.$value.push(b.$value[i])
          }
        }
      }
    }
  }
  return false
}

const isNotEqualAndIsEqual = (a, b, schema) => {
  if (a.$value === b.$value) {
    throw new Error(
      `Cannot have something equal and inequal @${a.$field} (${a.$operator}${a.$value}) and (${b.$operator}${b.$value})`
    )
  }
  return false
}

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
    const prevFilter = a[i]
    if (filterExists(prevFilter, filter)) {
      return false
    }

    const $a = prevFilter.$operator
    const $b = filter.$operator

    let fn
    if ($a === '=' && $b === '=') {
      fn = isEqual
    } else if ($a === '!=' && $b === '!=') {
      fn = isNotEqual
    } else if (($a === '=' && $b === '!=') || ($a === '!=' && $b === '=')) {
      fn = isNotEqualAndIsEqual
    } else if (($a === '>' && $b === '<') || ($a === '<' && $b === '>')) {
      fn = isLargerThenAndSmallerThen
    } else if ($a === '>' && $b === '>') {
      fn = isLargerThenAndLargerThen
    } else if ($a === '<' && $b === '<') {
      fn = isSmallerThenAndSmallerThen
    } else if (
      // auto merge smaller then and equal then arrays or just dont allow them
      ($a === '..' && ($b === '>' || $b === '<')) ||
      ($b === '..' && ($a === '>' || $a === '<'))
    ) {
      console.log('RANGE', $a, $b)
      fn = isRangeAndLargerOrSmaller
    }
    if (fn(prevFilter, filter, schema) === false) {
      return false
    }
  }

  return filter
}

export default compareFilters
