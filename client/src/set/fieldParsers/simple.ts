import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { TypeSchema, Schema, FieldSchemaOther } from '../../schema'
import digest from '../../digest'
import { incrementDef, incrementDoubleDef, longLongDef, doubleDef } from '../modifyDataRecords'

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
  timestamp: (payload: 'now' | number): Buffer => createRecord(longLongDef, { d: BigInt(payload === 'now' ? Date.now() : payload) }),
  boolean: (v: boolean): Buffer => createRecord(longLongDef, { d: BigInt(+v) }),
  int: (payload: number): Buffer => createRecord(longLongDef, { d: BigInt(payload) }),
  float: (payload: number): Buffer => createRecord(doubleDef, { d: payload }),
  number: (payload: number): Buffer => createRecord(doubleDef, { d: payload }),
}

const parsers = {}

for (const key in verifiers) {
  const verify = verifiers[key]
  const valueType: string =
    ['boolean', 'int', 'timestamp'].includes(key)
      ? '3'
      : ['float', 'number'].includes(key) ? 'A' : '0'
  const isNumber = key === 'float' || key === 'number' || key === 'int'
  const noOptions = key === 'type' || key === 'id' || key === 'digest'
  const converter = converters[key]

  parsers[key] = (
    _client: SelvaClient,
    _schemas: Schema,
    field: string,
    payload: SetOptions,
    result: (string | Buffer)[],
    _fields: FieldSchemaOther,
    _type: string
  ) => {
    let keyname: string = field
    let value: string | null = null

    if (!noOptions && typeof payload === 'object') {
      let hasKeys = false
      for (let k in payload) {
        value = payload[k]
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

            value = `___selva_$ref:${payload[k].$ref}`
          } else {
            if (!verify(payload[k])) {
              throw new Error(`Incorrect payload for ${key}.${k} ${payload}`)
            } else if (
                converter &&
                !['int', 'float', 'number'].includes(key) // createRecord will take care of numbers
            ) {
              value = converter(payload[k])
            }

            if (k !== '$value') {
              //console.log(payload)
              if (key == 'int') {
                result.push(
                  '4',
                  field,
                  createRecord(incrementDef, {
                    $default: isNaN(payload.$default)
                      ? 0
                      : Number(payload.$default),
                    $increment: isNaN(payload.$increment)
                      ? 0
                      : Number(payload.$increment)
                  })
                )
              } else {
                result.push(
                  'B',
                  field,
                  createRecord(incrementDoubleDef, {
                    $default: isNaN(payload.$default)
                      ? 0
                      : Number(payload.$default),
                    $increment: isNaN(payload.$increment)
                      ? 0
                      : Number(payload.$increment)
                  })
                )
              }
              return
            }
          }
        } else if (k === '$ref') {
          value = `___selva_$ref:${payload[k]}`
          return
        } else if (k === '$delete') {
          result.push('7', field, '')
          return
        } else {
          throw new Error(`Incorrect payload for ${key} incorrect field ${k}`)
        }
      }

      if (!hasKeys) {
        throw new Error(`Incorrect payload empty object for field ${field}`)
      }
    } else if (verify(payload)) {
      if (converter) {
        value = converter(payload)
      } else {
        value = String(payload)
      }
    } else {
      throw new Error(
        `Incorrect payload for ${field} of type ${key}: ${JSON.stringify(
          payload
        )}`
      )
    }

    if (value !== null && value !== undefined) {
      result.push(valueType, keyname, value)
    }
  }
}

export default parsers
