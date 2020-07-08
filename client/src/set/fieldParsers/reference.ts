import parseSetObject from '../validate'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike } from '../../schema'
import { verifiers } from './simple'

const id = verifiers.id

const verifySimple = (payload: any, schemas: Schema) => {
  if (id(payload, schemas)) {
    return payload
  } else {
    throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
  }
}

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  _fields: FieldSchemaArrayLike,
  _type: string,
  $lang?: string
): void => {
  if (typeof payload === 'object') {
    if (Array.isArray(payload)) {
      throw new Error(
        `Wrong payload for reference ${JSON.stringify(
          payload
        )}, should be an object or id string`
      )
    }

    if (payload.$delete === true) {
      result[field] = { $delete: true }
      return
    }

    result[field] = parseSetObject(payload, schema, $lang)
  } else {
    if (typeof payload !== 'string') {
      throw new Error(
        `Wrong payload for reference ${JSON.stringify(
          payload
        )}, should be an object or id string`
      )
    }

    result[field] = verifySimple(payload, schema)
  }
}
