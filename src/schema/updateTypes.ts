import { Types } from './'

// small caps counter (2 spaces)
const uid = num => {
  const div = (num / 26) | 0
  var str = String.fromCharCode(97 + (num % 26))
  if (div) {
    if ((div / 26) | 0) {
      str = str + uid(div)
    } else {
      str = str + String.fromCharCode(97 + (div % 26))
    }
  }
  return str
}

const findKey = (obj: { [key: string]: any }, value: any): false | string => {
  for (let k in obj) {
    if (obj[k] === value) {
      return k
    }
  }
  return false
}

async function parseTypes(props: Types, types: Record<string, string>) {
  // types
  for (let type in props.types) {
    const definition = props.types[type]
    if (!types[type]) {
      if (definition.prefix) {
        const exists = findKey(types, definition.prefix)
        if (exists) {
          throw new Error(
            `Prefix allready exists ${definition.prefix} ${exists}`
          )
        }
        types[type] = definition.prefix
      } else {
        // generate one your self
      }
    } else {
      if (definition.prefix !== types[type]) {
        throw new Error(
          `Trying to change prefix of ${type} from ${types[type]} to ${definition.prefix}`
        )
      }
    }
  }
}

export default parseTypes
