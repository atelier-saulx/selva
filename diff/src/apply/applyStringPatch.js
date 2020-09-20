const applyPatch = (prevValue, patch, shouldbe) => {
  const len = patch.length
  let newStr = ''
  let cursor = 0
  for (let i = 0; i < len; i++) {
    const [o, v] = patch[i]
    if (o === 0) {
      newStr += prevValue.slice(cursor, v + cursor)
      cursor += v
    } else if (o === 1) {
      newStr += v
    } else if (o === 2) {
      cursor += v
    }
  }
  if (newStr !== shouldbe) {
    console.error('NOOOO')
  }
  return newStr
}

module.exports = applyPatch
