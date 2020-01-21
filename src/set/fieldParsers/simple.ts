import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaOther } from '../../schema'

const isUrlRe = new RegExp(
  '^(https?:\\/\\/)?' + // protocol
  '((([a-z\\d]([a-z\\d-]*[a-z\\d])*)\\.)+[a-z]{2,}|' + // domain name
  '((\\d{1,3}\\.){3}\\d{1,3}))' + // OR ip (v4) address
  '(\\:\\d+)?(\\/[-a-z\\d%_.~+]*)*' + // port and path
  '(\\?[;&a-z\\d%_.~+=-]*)?' + // query string
    '(\\#[-a-z\\d_]*)?$',
  'i'
)

const validURL = (str: string): boolean => {
  return isUrlRe.test(str)
}

const verifiers = {
  string: (payload: string) => {
    return typeof payload === 'string'
  },
  url: (payload: string) => {
    return typeof payload === 'string' && validURL(payload)
  },
  number: (payload: number) => {
    return typeof payload === 'number'
  },
  float: (payload: number) => {
    return typeof payload === 'number'
  },
  int: (payload: number) => {
    return typeof payload === 'number'
  }
}

const parsers = {
  type: (
    schemas: Record<string, TypeSchema>,
    field: string,
    payload: string,
    result: SetOptions,
    fields: FieldSchemaOther,
    type: string
  ): void => {
    if (typeof payload === 'string' && payload.length < 20) {
      result[field] = payload
    } else {
      throw new Error(
        `Type needs to be a string no longer then 20 chars ${payload}`
      )
    }
  }
}

for (const key in verifiers) {
  const verify = verifiers[key]
  const isNumber = key === 'float' || key === 'number' || key === 'int'
  parsers[key] = (
    schemas: Record<string, TypeSchema>,
    field: string,
    payload: SetOptions,
    result: SetOptions,
    fields: FieldSchemaOther,
    type: string
  ) => {
    if (typeof payload === 'object') {
      for (let k in payload) {
        if (
          k === '$default' ||
          k === '$value' ||
          (isNumber && k === '$increment')
        ) {
          if (!verify(payload[k])) {
            throw new Error(`Incorrect payload for ${key}.${k} ${payload}`)
          }
        } else {
          throw new Error(`Incorrect payload for ${key} incorrect field ${k}`)
        }
      }
      result[field] = payload
    } else if (verify(payload)) {
      result[field] = payload
    } else {
      throw new Error(`Incorrect payload for ${key} ${payload}`)
    }
  }
}

export default parsers
