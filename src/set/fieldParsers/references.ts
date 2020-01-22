import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaArrayLike } from '../../schema'
import { parseSetObject } from '../'

type Schemas = Record<string, TypeSchema>

const verifySimple = payload => {
  if (Array.isArray(payload)) {
    if (payload.find(v => typeof v !== 'string')) {
      throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
    }
    return payload
  } else if (typeof payload === 'string') {
    return [payload]
  } else {
    throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
  }
}

const parseObjectArray = (payload, schemas: Schemas) => {
  if (Array.isArray(payload) && typeof payload[0] === 'object') {
    return payload.map(ref => parseSetObject(ref, schemas))
  }
}

export default (
  schemas: Record<string, TypeSchema>,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string
): void => {
  if (typeof payload === 'object' && !Array.isArray(payload)) {
    result[field] = {}
    for (let k in payload) {
      if (k === '$add') {
        const parsed = parseObjectArray(payload[k], schemas)
        if (parsed) {
          result[field].$add = parsed
        } else if (
          typeof payload[k] === 'object' &&
          !Array.isArray(payload[k])
        ) {
          result[field].$add = parseSetObject(payload[k], schemas)
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
    result[field] = parseObjectArray(payload, schemas) || verifySimple(payload)
  }
}
