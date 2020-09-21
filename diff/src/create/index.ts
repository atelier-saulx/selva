// 2 something with arrays
const compareNode = (a, b, result, key) => {
  const type = typeof b
  if (type !== typeof a) {
    // different type good start
    // strong
    console.info('type is different', a, b)
    result[key] = [0, b]
  } else if (type === 'object') {
    if (b === null) {
      result[key] = b
    } else {
      const r = {}
      if (b.constructor === Array) {
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
            result[key] = [1]
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
      result[key] = [0, b]
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
            result[key] = [1]
          }
        }
        return result
      }
    }
  } else {
    if (a === b) {
      // no change do nothing
    } else {
      //   if (type === 'string') {
      //   } else {
      //     // add change
      //   }
      return [0, b]
    }
  }
}

export default compare
