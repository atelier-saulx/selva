const nestedApplyPatch = (value: object, key: string, patch) => {
  if (patch.constructor === Array) {
    const type = patch[0]
    // 0 - insert
    // 1 - remove
    // 2 - array
    if (type === 0) {
      value[key] = patch[1]
    } else if (type === 1) {
      delete value[key]
    } else if (type === 2) {
      value[key] = applyArrayPatch(value[key], patch[1])
    }
  } else {
    for (let nkey in patch) {
      nestedApplyPatch(value[key], nkey, patch[nkey])
    }
  }
}

// make types
// hash
// nested object
const applyArrayPatch = (value: any[], arrayPatch) => {
  //   console.log('go do array!', arrayPatch)
  const patchLength = arrayPatch.length
  const newArray = new Array(arrayPatch[0])
  let aI = -1
  for (let i = 1; i < patchLength; i++) {
    // 0 - insert, value
    // 1 - from , index, amount (can be a copy a well)
    // 2 - amount, index
    const operation = arrayPatch[i]
    const type = operation[0]
    if (type === 0) {
      for (let j = 1; j < operation.length; j++) {
        newArray[++aI] = operation[j]
      }
    } else if (type === 1) {
      const piv = operation[2]
      const range = operation[1] + piv
      for (let j = piv; j < range; j++) {
        newArray[++aI] = value[j]
      }
    } else if (type === 2) {
      // nested diff
      // generate the hash in the create patch
      const piv = operation[1]
      const range = operation.length - 2 + piv
      for (let j = piv; j < range; j++) {
        newArray[++aI] = applyPatch(value[j], operation[j - piv + 2])
      }
    }
  }

  return newArray
  // can also be nested
}

const applyPatch = (value, patch) => {
  if (patch) {
    if (patch.constructor === Array) {
      const type = patch[0]
      // 0 - insert
      // 1 - remove
      // 2 - array
      if (type === 0) {
        return patch[1]
      } else if (type === 1) {
        return undefined
      } else if (type === 2) {
        return applyArrayPatch(value, patch[1])
      }
    } else {
      for (let key in patch) {
        nestedApplyPatch(value, key, patch[key])
      }
      return value
    }
  } else {
    // console.log('NO CHANGE IN APPLY PATCH!')
    return value
  }
}

export default applyPatch
