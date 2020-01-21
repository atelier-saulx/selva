import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaArrayLike } from '../../schema'
import { parseSetObject } from '../'

// make this nice
const checkArrayLikeField = payload => {
  if (Array.isArray(payload)) {
    if (payload.find(ref => typeof ref !== 'string')) {
      throw new Error(
        `Incorrect payload for references  ${JSON.stringify(payload)}`
      )
    }
  } else if (typeof payload !== 'string') {
    throw new Error(
      `Incorrect payload for references  ${JSON.stringify(payload)}`
    )
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
    if (typeof payload[0] === 'object') {
      result[field] = []
      payload.forEach(ref => {
        result[field].push(parseSetObject(ref, schemas))
      })
    } else {
      checkArrayLikeField(payload)
      result[field] = payload
    }
  } else if (typeof payload === 'object') {
    // check if its correct

    if (payload.$remove) {
      checkArrayLikeField(payload.$remove)
    }

    if (payload.$add) {
      if (typeof payload.$add === 'object') {
        payload.$add = parseSetObject(payload.$add, schemas)
        for (let key in payload) {
          if (key !== '$add') {
            result[field][key] = payload[key]
          }
        }
      } else {
        checkArrayLikeField(payload.$add)
      }
    }

    result[field] = payload
  } else if (typeof payload === 'string') {
    result[field] = payload
  } else {
    throw new Error(
      `Incorrect payload for references ${field} ${JSON.stringify(payload)}`
    )
  }
}
