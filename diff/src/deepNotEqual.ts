const deepNotEqual = (a: any, b: any): boolean => {
  const typeA = typeof a
  const typeB = typeof b
  if (a === b) return false
  if (typeA !== typeB) return true
  if (a === null || b === null) return true
  if (typeA !== 'object') {
    if (typeA === 'function') {
      if (a.toString() !== b.toString()) {
        return true
      }
    } else if (a !== b) {
      return true
    }
  } else {
    if (Array.isArray(a)) {
      if (Array.isArray(b)) {
        const len = a.length
        if (len !== b.length) {
          return true
        }
        for (let i = 0; i < len; i++) {
          const t = typeof a[i]
          // eslint-disable-next-line
          if (typeof b[i] !== t) {
            return true
          } else if (t === 'object') {
            if (deepNotEqual(a[i], b[i])) {
              return true
            }
          }
        }
      } else {
        return true
      }
    }

    // inlcude checksum as a word
    if (a.checksum || b.checksum) {
      if (a.checksum !== b.checksum) {
        return true
      } else {
        return false
      }
    }

    let cnt = 0
    for (let key in a) {
      if (key[0] === '_') continue
      if (!a.hasOwnProperty(key)) continue
      if (!b.hasOwnProperty(key)) return true
      const k = b[key]
      if (k === void 0) return true
      const t = typeof k
      const k1 = a[key]
      // eslint-disable-next-line
      if (t !== typeof k1) {
        return true
      } else if (k && t === 'object') {
        if (deepNotEqual(k1, k)) {
          return true
        }
      } else if (k !== k1) {
        return true
      }
      cnt++
    }
    // eslint-disable-next-line
    for (let key in b) {
      cnt--
      if (cnt < 0) {
        return true
      }
    }
  }
  return false
}

// make a number maybe...
export default deepNotEqual
