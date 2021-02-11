import { deepCopy } from '@saulx/utils'

const nestedApplyPatch = (
  value: Object | Array<any>,
  key: string,
  patch
): void | null => {
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
      const r = applyArrayPatch(value[key], patch[1])
      if (r === null) {
        return null
      }
      value[key] = r
    }
  } else {
    if (patch.___$toObject && value[key] && value[key].constructor === Array) {
      const v = {}
      for (let i = 0; i < value[key].length; i++) {
        v[i] = value[key][i]
      }
      value[key] = v
    }

    if (value[key] === undefined) {
      console.warn(
        'Diff apply patch: Cannot find key in original object',
        key,
        JSON.stringify(patch, null, 2)
      )
      return null
      // lets throw
    } else {
      for (const nkey in patch) {
        if (
          nkey !== '___$toObject' &&
          nestedApplyPatch(value[key], nkey, patch[nkey]) === null
        ) {
          return null
        }
      }
    }
  }
}

const applyArrayPatch = (value: any[], arrayPatch): any[] | null => {
  const patchLength = arrayPatch.length
  const newArray = new Array(arrayPatch[0])
  let aI = -1

  const copied = []
  const patches = []

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

      copied.push(operation)

      for (let j = piv; j < range; j++) {
        newArray[++aI] = value[j]
      }
    } else if (type === 2) {
      // nested diff
      // generate the hash in the create patch
      const piv = operation[1]
      const range = operation.length - 2 + piv
      for (let j = piv; j < range; j++) {
        ++aI
        patches.push([aI, j, operation[j - piv + 2]])
      }
    }
  }

  const len = patches.length
  for (let i = 0; i < len; i++) {
    const [aI, j, patch] = patches[i]
    let needsCopy = false
    for (let k = 0; k < copied.length; k++) {
      const [_, a, b] = copied[k]
      if (j >= b && j < b + a) {
        needsCopy = true
        break
      }
    }
    if (needsCopy) {
      const copy = deepCopy(value[j])
      const newObject = applyPatch(copy, patch)
      if (newObject === null) {
        return null
      }
      newArray[aI] = newObject
    } else {
      const newObject = applyPatch(value[j], patch)
      if (newObject === null) {
        return null
      }
      newArray[aI] = newObject
    }
  }

  return newArray
  // can also be nested
}

const applyPatch = (value, patch): any | null => {
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
      if (patch.___$toObject && value && value.constructor === Array) {
        const v = {}
        for (let i = 0; i < value.length; i++) {
          v[i] = value[i]
        }
        value = v
      }
      for (const key in patch) {
        if (key !== '___$toObject') {
          const r = nestedApplyPatch(value, key, patch[key])
          if (r === null) {
            return null
          }
        }
      }
      return value
    }
  } else {
    return value
  }
}

export default applyPatch
