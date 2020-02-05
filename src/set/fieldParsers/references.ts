import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaArrayLike } from '../../schema'
import { parseSetObject } from '../'
import { verifiers } from './simple'

const id = verifiers.id
type Schemas = Record<string, TypeSchema>

const verifySimple = payload => {
  if (Array.isArray(payload)) {
    if (payload.find(v => !id(v))) {
      throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
    }
    return payload
  } else if (id(payload)) {
    return [payload]
  } else {
    throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
  }
}

const parseObjectArray = (payload: any, schema: Schema) => {
  if (Array.isArray(payload) && typeof payload[0] === 'object') {
    return payload.map(ref => parseSetObject(ref, schema))
  }
}

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  _fields: FieldSchemaArrayLike,
  _type: string
): void => {
  console.log(field)
  if (typeof payload === 'object' && !Array.isArray(payload)) {
    result[field] = {}
    for (let k in payload) {
      if (k === '$add') {
        const parsed = parseObjectArray(payload[k], schema)
        if (parsed) {
          result[field].$add = parsed
        } else if (
          typeof payload[k] === 'object' &&
          !Array.isArray(payload[k])
        ) {
          result[field].$add = [parseSetObject(payload[k], schema)]
        } else {
          result[field].$add = verifySimple(payload[k])
        }
      } else if (k === '$delete') {
        result[field].$delete = verifySimple(payload[k])
      } else if (k === '$hierarchy') {
        if (payload[k] !== false && payload[k] !== true) {
          throw new Error(
            `Wrong payload for references ${JSON.stringify(payload)}`
          )
        }
        result[field].$hierarchy = payload[k]
      } else {
        throw new Error(`Wrong key for references ${k}`)
      }
    }
  } else {
    result[field] = parseObjectArray(payload, schema) || verifySimple(payload)
  }
}
