import { Schema, Types } from '~selva/schema/index'
import { testString } from '../util'
import * as logger from '../logger'

const validate = (id: string): boolean => {
  return testString(id, '[%a%d][%a%d]')
}

// - means id seperator
const findKey = (obj: Types, value: any): false | string => {
  for (let k in obj) {
    if (obj[k].prefix === value) {
      return k
    }
  }
  return false
}

const hasDuplicates = (obj: Types, value: string): false | [string, string] => {
  let found = false
  let result: [string, string] = <[string, string]>(<unknown>[]) // force tuple typecheck for nicer code to build result value
  for (let k in obj) {
    if (obj[k].prefix === value) {
      if (found) {
        result[1] = k
        return result
      }

      result[0] = k
      found = true
    }
  }
  return false
}

function fromCharCode(n: number): string {
  n += 48
  if (n > 57) {
    n += 7
  }
  if (n > 90) {
    n += 6
  }
  return string.char(n)
}

function rndStr(seed: number): string {
  const div = Math.floor(seed / 62)
  let str = fromCharCode(seed % 62)
  if (div) {
    if (Math.floor(div / 62) !== 0) {
      str = str + rndStr(div)
    } else {
      str = str + fromCharCode(div % 62)
    }
  }
  return str
}

function genPrefix(schema: Schema, type: string): string {
  schema.idSeedCounter = schema.idSeedCounter ? schema.idSeedCounter + 1 : 1
  let id = rndStr(schema.idSeedCounter)
  if (id.length === 1) {
    id = type[0] + id
  }
  if (findKey(schema.types, id)) {
    return genPrefix(schema, type)
  }
  return id
}

export default function ensurePrefixes(
  oldSchema: Schema,
  newSchema: Schema
): string | null {
  for (let type in newSchema.types) {
    const definition = newSchema.types[type]

    if (oldSchema.types[type]) {
      if (
        definition.prefix &&
        definition.prefix !== oldSchema.types[type].prefix
      ) {
        return `Can not change prefix of type ${type}`
      }
    }

    if (definition.prefix) {
      if (!validate(definition.prefix)) {
        return `Prefix wrongly formatted ${definition.prefix} make it longer then 2 chars and a combination of (a-z A-Z 0-9)`
      }

      let duplicates: false | [string, string]
      if ((duplicates = hasDuplicates(newSchema.types, definition.prefix))) {
        return `Duplicate types found for prefix ${definition.prefix}: ${duplicates[0]} and ${duplicates[1]}`
      }
    } else {
      definition.prefix = genPrefix(newSchema, type)
    }
  }

  return null
}
