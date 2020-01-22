import { Types, TypesDb } from '.'
import { SelvaClient } from '..'

const fromCharCode = n => {
  n += 48
  if (n > 57) {
    n += 7
  }
  if (n > 90) {
    n += 6
  }
  return String.fromCharCode(n)
}

const uid = (num: number): string => {
  const div = (num / 62) | 0
  var str = fromCharCode(num % 62)
  if (div) {
    if ((div / 62) | 0) {
      str = str + uid(div)
    } else {
      str = str + fromCharCode(div % 62)
    }
  }
  return str
}

// - means id seperator
const findKey = (obj: { [key: string]: any }, value: any): false | string => {
  for (let k in obj) {
    if (obj[k] === value) {
      return k
    }
  }
  return false
}

const validate = (id: string): boolean => {
  return /[a-z]{2}/.test(id)
}

const genId = (types: TypesDb, type: string): string => {
  types.idSize++
  let id = uid(types.idSize)
  if (id.length === 1) {
    id = type[0] + id
  }
  if (findKey(types, id)) {
    return genId(types, type)
  }
  return id
}

async function parseTypes(client: SelvaClient, props: Types, types: TypesDb) {
  let changed: boolean = false
  for (let type in props) {
    const definition = props[type]
    if (!types[type]) {
      if (definition.prefix) {
        if (!validate(definition.prefix)) {
          throw new Error(
            `Prefix wrongly formatted ${definition.prefix} make it lower case letters and not longer then 2 chars`
          )
        }
        const exists = findKey(types, definition.prefix)
        if (exists) {
          throw new Error(
            `Prefix already exists ${definition.prefix} ${exists}`
          )
        }
        types[type] = definition.prefix
      } else {
        types[type] = genId(types, type)
      }
      changed = true
    } else {
      if (definition.prefix && definition.prefix !== types[type]) {
        throw new Error(
          `Trying to change prefix of ${type} from ${types[type]} to ${definition.prefix}`
        )
      }
    }
  }
  if (changed) {
    await client.redis.hset('schema', 'types', JSON.stringify(types))
    const prefixes = {}
    for (let key in types) {
      if (key !== 'idSize') {
        prefixes[types[key]] = key
      }
    }
    await client.redis.hset('schema', 'prefixes', JSON.stringify(prefixes))
  }
}

export default parseTypes
