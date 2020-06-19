import { SetOptions } from '../types'
import { TypeSchema, Schema, FieldSchemaOther } from '../../schema'
import digest from '../../digest'

const isUrlRe = new RegExp(
  '^(?!mailto:)(?:(?:http|https|ftp)://)(?:\\S+(?::\\S*)?@)?(?:(?:(?:[1-9]\\d?|1\\d\\d|2[01]\\d|22[0-3])(?:\\.(?:1?\\d{1,2}|2[0-4]\\d|25[0-5])){2}(?:\\.(?:[0-9]\\d?|1\\d\\d|2[0-4]\\d|25[0-4]))|(?:(?:[a-z\\u00a1-\\uffff0-9]+-?)*[a-z\\u00a1-\\uffff0-9]+)(?:\\.(?:[a-z\\u00a1-\\uffff0-9]+-?)*[a-z\\u00a1-\\uffff0-9]+)*(?:\\.(?:[a-z\\u00a1-\\uffff]{2,})))|localhost)(?::\\d{2,5})?(?:(/|\\?|#)[^\\s]*)?$',
  'i'
)

const validURL = (str: string): boolean => {
  return isUrlRe.test(str)
}

/*
 | 'id'
  | 'digest'
  | 'timestamp'
  | 'url'
  | 'email'
  | 'phone'
  | 'geo' - still missing
  | 'type'
*/

export const verifiers = {
  digest: (payload: string) => {
    return typeof payload === 'string'
  },
  string: (payload: string) => {
    return typeof payload === 'string'
  },
  phone: (payload: string) => {
    // phone is wrong
    return typeof payload === 'string' && payload.length < 30
  },
  timestamp: (payload: 'now' | number) => {
    return (
      payload === 'now' ||
      (typeof payload === 'number' && Number.isInteger(payload) && payload > 0)
    )
  },
  url: (payload: string) => {
    return typeof payload === 'string' && validURL(payload)
  },
  email: (payload: string) => {
    const re = /^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/
    return re.test(payload.toLowerCase())
  },
  number: (payload: number) => {
    return typeof payload === 'number'
  },
  boolean: (payload: boolean) => {
    return typeof payload === 'boolean'
  },
  float: (payload: number) => {
    return typeof payload === 'number'
  },
  int: (payload: number) => {
    return typeof payload === 'number'
  },
  type: (payload: string) => {
    return typeof payload === 'string' && payload.length < 20
  },
  id: (payload: string) => {
    return typeof payload === 'string' && payload.length < 20
  }
}

// also need to make this accessable
const converters = {
  digest,
  timestamp: (payload: 'now' | number): number => {
    if (payload === 'now') {
      return Date.now()
    } else {
      return payload
    }
  }
}

const parsers = {}

for (const key in verifiers) {
  const verify = verifiers[key]
  const isNumber = key === 'float' || key === 'number' || key === 'int'
  const noOptions = key === 'type' || key === 'id' || key === 'digest'
  const converter = converters[key]

  parsers[key] = (
    _schemas: Schema,
    field: string,
    payload: SetOptions,
    result: SetOptions,
    _fields: FieldSchemaOther,
    _type: string
  ) => {
    if (!noOptions && typeof payload === 'object') {
      let hasKeys = false
      for (let k in payload) {
        hasKeys = true
        if (
          k === '$default' ||
          k === '$value' ||
          (isNumber && k === '$increment')
        ) {
          if (payload[k].$ref) {
            if (typeof payload[k].$ref !== 'string') {
              throw new Error(`Non-string $ref provided for ${key}.${k}`)
            }
            payload[k] = `___selva_$ref:${payload[k].$ref}`
          } else {
            if (!verify(payload[k])) {
              throw new Error(`Incorrect payload for ${key}.${k} ${payload}`)
            } else if (converter) {
              payload[k] = converter(payload[k])
            }
          }
        } else if (k === '$ref') {
          // TODO: verify it references the same type
          result[field] = `___selva_$ref:${payload[k]}`
          return
        } else if (k === '$delete') {
          result[field] = { $delete: true }
        } else {
          throw new Error(`Incorrect payload for ${key} incorrect field ${k}`)
        }
      }

      if (!hasKeys) {
        throw new Error(`Incorrect payload empty object for field ${field}`)
      }
      result[field] = payload
    } else if (verify(payload)) {
      result[field] = payload
      if (converter) {
        result[field] = converter(payload)
      }
    } else {
      throw new Error(
        `Incorrect payload for ${field} of type ${key}: ${JSON.stringify(
          payload
        )}`
      )
    }
  }
}

export default parsers
