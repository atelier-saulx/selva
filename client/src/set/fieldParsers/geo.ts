import { SetOptions } from '../types'
import { Schema, FieldSchemaOther } from '../../schema'

function refs(field: string, payload: SetOptions): void {
  if (payload.$ref && Object.keys(payload).length !== 1) {
    throw new Error(`$ref only allow without other fields ${field} ${payload}`)
  }

  for (const field of ['lat', 'long']) {
    if (payload[field].$default && payload[field].$default.$ref) {
      payload[field].$default = `___selva_$ref:${payload[field].$default.$ref}`
    } else if (payload[field].$ref) {
      payload[field] = `___selva_$ref:${payload[field].$ref}`
    }
  }
}

function verify(payload: any) {
  const keys = Object.keys(payload)
  if (keys.length !== 2 || !payload.lat || !payload.long) {
    return false
  }

  if (payload.lat < -90 || payload.lat > 90) {
    return false
  }

  if (payload.long < -180 || payload.long > 180) {
    return false
  }
}

export default (
  _schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  _fields: FieldSchemaOther,
  _type: string
): void => {
  refs(field, payload)
  verify(payload)
  result[field] = payload
}
