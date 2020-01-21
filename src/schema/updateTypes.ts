import { Types, TypesDb } from './'

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

// - means id seperator
// else first 2 letters
//check if - in id else use first 2
// [flurp-] (this becomes the id)
// ids cant have - in them!

const findKey = (obj: { [key: string]: any }, value: any): false | string => {
  for (let k in obj) {
    if (obj[k] === value) {
      return k
    }
  }
  return false
}

const validate = id => {
  return /[a-z]{1,10}/.test(id)
}

async function parseTypes(props: Types, types: TypesDb) {
  // types
  for (let type in props.types) {
    const definition = props.types[type]
    if (!types[type]) {
      if (definition.prefix) {
        if (!validate(definition.pefix)) {
          throw new Error(
            `Prefix wrongly formatted ${definition.prefix} make it lower case letters and not longer then 10 chars`
          )
        }

        const exists = findKey(types, definition.prefix)
        if (exists) {
          throw new Error(
            `Prefix allready exists ${definition.prefix} ${exists}`
          )
        }
        types[type] = definition.prefix
      } else {
        types.idSize++
        // uid()
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
