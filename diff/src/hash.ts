const djb2 = (str, hash = 5381) => {
  var i = str.length
  while (i) {
    hash = (hash * 33) ^ str.charCodeAt(--i)
  }
  return hash
}

const argHasher = (obj, hash = 5381) => {
  for (let key in obj) {
    const field = obj[key]
    const type = typeof field
    if (type === 'string') {
      hash = (djb2(field, hash) * 33) ^ djb2(key, hash)
    } else if (type === 'number') {
      hash = (((hash * 33) ^ field) * 33) ^ djb2(key, hash)
    } else if (type === 'object') {
      if (field !== null) {
        hash = argHasher(field, hash)
      }
    } else if (type === 'boolean') {
      hash = (((hash * 33) ^ (field === true ? 1 : 0)) * 33) ^ djb2(key, hash)
    }
  }
  return hash
}

const hash = (props: object) => {
  return argHasher(props) >>> 0
}

export default hash
