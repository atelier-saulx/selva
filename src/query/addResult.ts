const addToResult = (result, filter, type) => {
  const field = filter.$field
  result.filters[type].push(filter)
  if (!result.reverseMap[field]) {
    result.reverseMap[field] = []
  }
  result.reverseMap[field].push(filter)
}

export default addToResult
