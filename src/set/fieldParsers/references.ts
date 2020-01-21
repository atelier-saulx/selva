import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaArrayLike } from '../../schema'
import { parseSetObject } from '../'

type Schemas = Record<string, TypeSchema>

const verifySimple = payload => {
  if (Array.isArray(payload)) {
    return !payload.find(v => typeof v !== 'string')
  } else if (typeof payload === 'string') {
    return true
  } else {
    return false
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
  if (Array.isArray(payload)) {
    const parsed = parseObjectArray(payload, schemas)
    if (parsed) {
      result[field] = parsed
    } else if (verifySimple(payload)) {
      result[field] = payload
    } else {
      throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
    }
  } else if (typeof payload === 'string') {
    result[field] = payload
  } else if (typeof payload === 'object') {
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
        } else if (verifySimple(payload[k])) {
          result[field].$add = payload[k]
        } else {
          throw new Error(`Wrong payload for ${k}`)
        }
      } else if (k === '$remove') {
        if (!verifySimple(payload[k])) {
          throw new Error(`Wrong payload for ${k}`)
        }
      } else if (k === '$hierarchy') {
        if (payload[k] !== false && payload[k] !== true) {
          throw new Error(`Wrong payload for ${k}`)
        }
      } else {
        throw new Error(`Wrong key for references ${k}`)
      }
    }
  } else {
    throw new Error(`Wrong type for references ${JSON.stringify(payload)}`)
  }
}
