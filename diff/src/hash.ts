export const stringHash = (str, hash = 5381): number => {
  var i = str.length
  while (i) {
    hash = (hash * 33) ^ str.charCodeAt(--i)
  }
  return hash
}

export const hashObjectNest = (obj, hash = 5381): number => {
  for (let key in obj) {
    const field = obj[key]
    const type = typeof field
    if (type === 'string') {
      hash = (stringHash(field, hash) * 33) ^ stringHash(key, hash)
    } else if (type === 'number') {
      hash = (((hash * 33) ^ field) * 33) ^ stringHash(key, hash)
    } else if (type === 'object') {
      if (field === null) {
        hash = 5381 ^ stringHash(key, hash)
      } else {
        hash = (hashObjectNest(field, hash) * 33) ^ stringHash(key, hash)
      }
    } else if (type === 'boolean') {
      hash =
        (((hash * 33) ^ (field === true ? 1 : 0)) * 33) ^ stringHash(key, hash)
    }
  }
  return hash
}

export const hashObject = (props: object): number => {
  return hashObjectNest(props) >>> 0
}

const hash = (val: any): number => {
  if (typeof val === 'object') {
    if (val === null) {
      return 0
    } else {
      return hashObject(val)
    }
  } else {
    if (typeof val === 'number') {
      return (5381 * 33) ^ val
    } else {
      return stringHash(val)
    }
  }
}

export default hash
