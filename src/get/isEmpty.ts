const isEmpty = (value: any, def: any): boolean => {
  if (value === null || value === undefined || value === '') {
    return true
  } else if (Array.isArray(value) && value.length === 0 && Array.isArray(def)) {
    return true
  } else if (typeof value === 'object') {
    for (const key in value) {
      if (!isEmpty(value[key], def)) {
        return false
      }
    }
    return true
  }
  return false
}

export default isEmpty
