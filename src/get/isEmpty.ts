// should not use this but returns in get (if its complete from props)
const isEmpty = (value: any): boolean => {
  // better to use types for this and generate it...
  if (value === null || value === undefined || value === '') {
    return true
  } else if (Array.isArray(value) && value.length === 0) {
    return true
  } else if (typeof value === 'object') {
    for (const key in value) {
      if (!isEmpty(value[key])) {
        return false
      }
    }
    return true
  }
  return false
}

export default isEmpty
