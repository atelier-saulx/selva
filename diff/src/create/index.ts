// 2 = delete
// 3 = insert in array
// 0 = add

// array + object are all the things in here

const compareNode = (a, b, result, key) => {
  const type = typeof b
  if (type !== typeof a) {
    // different type good start
    // strong
    console.info('type is different', a, b)
    result[key] = b
  } else if (type === 'object') {
    if (b === null) {
      result[key] = b
    } else {
      const r = {}
      if (b.constructor === Array) {
        if (a.constructor === Array) {
          const bLen = b.length
          const aLen = a.length
          if (aLen > bLen) {
            for (let i = 0; i < bLen; i++) {
              compareNode(a[i], b[i], r, i)
            }
            r[bLen] = [2, aLen - bLen]
          } else if (aLen < bLen) {
            for (let i = 0; i < aLen; i++) {
              compareNode(a[i], b[i], r, i)
            }
            r[bLen] = [3, b.slice(aLen)]
          } else {
            for (let i = 0; i < bLen; i++) {
              compareNode(a[i], b[i], r, i)
            }
          }
        } else {
          result[key] = [0, b]
        }
      } else {
        for (const key in b) {
          if (!(key in a)) {
            r[key] = [0, b[key]]
          } else {
            compareNode(a[key], b[key], r, key)
          }
        }
        for (const key in a) {
          if (!(key in b)) {
            result[key] = [2]
          }
        }
      }

      // what does this mean?
      // checks if not empty...
      for (let x in r) {
        result[key] = r
        break
      }
    }
  } else {
    if (a === b) {
      // no change do nothing
    } else {
      if (type === 'string') {
        console.log('will add string later!')
      } else {
        // add change
      }
      result[key] = b
      // for now
    }
  }
}

const compare = (a, b) => {
  const type = typeof b
  // eslint-disable-next-line
  if (type !== typeof a) {
    return b
  } else if (type === 'object') {
    if (b === null) {
      return b
    } else {
      if (b.constructor === Array) {
        if (a.constructor === Array) {
          // check the differences!
        } else {
          return [0, b]
        }
      } else {
        const result = {}
        for (const key in b) {
          if (!(key in a)) {
            result[key] = [0, b[key]]
          } else {
            // same for a need to remove keys if b does not have them
            compareNode(a[key], b[key], result, key)
          }
        }
        for (const key in a) {
          if (!(key in b)) {
            result[key] = [2]
          }
        }
        return result
      }
    }
  } else {
    if (a === b) {
      // no change do nothing
    } else {
      if (type === 'string') {
      } else {
        // add change
      }
      return b
    }
  }
}

export default compare
