import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, FieldSchemaOther } from '../../schema'

function refs(field: string, payload: SetOptions): void {
  if (payload.$ref && Object.keys(payload).length !== 1) {
    throw new Error(`$ref only allow without other fields ${field} ${payload}`)
  }

  for (const field of ['lat', 'lon']) {
    if (payload[field].$default && payload[field].$default.$ref) {
      payload[field].$default = `___selva_$ref:${payload[field].$default.$ref}`
    } else if (payload[field].$ref) {
      payload[field] = `___selva_$ref:${payload[field].$ref}`
    }
  }
}

function verify(payload: any) {
  const keys = Object.keys(payload)
  if (keys.length !== 2 || !payload.lat || !payload.lon) {
    return false
  }

  if (payload.lat < -90 || payload.lat > 90) {
    return false
  }

  if (payload.lon < -180 || payload.lon > 180) {
    return false
  }
}

export default async (
  _client: SelvaClient,
  _schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  _fields: FieldSchemaOther,
  _type: string
): Promise<void> => {
  if (payload.$delete) {
    result[field] = { $delete: true }
    return
  }

  refs(field, payload)
  verify(payload)
  // result[field] = `${payload.lon},${payload.lat}`
  result.push('0', field, `${payload.lon},${payload.lat}`)
}
