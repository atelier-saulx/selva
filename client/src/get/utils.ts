import { FieldSchema, Schema } from '../schema'
import { GetResult } from './types'

export function getTypeFromId(schema: Schema, id: string): string | undefined {
  if (id.startsWith('ro')) {
    return 'root'
  }

  return schema.prefixToTypeMapping[id.substr(0, 2)]
}

export function getNestedSchema(
  schema: Schema,
  id: string,
  field: string
): FieldSchema | null {
  if (!field || field === '') {
    return null
  }

  const type = getTypeFromId(schema, id)
  const fields = field.split('.')

  const typeSchema = type === 'root' ? schema.rootType : schema.types[type]
  if (!typeSchema || !typeSchema.fields) {
    return null
  }

  let prop: any = typeSchema.fields[fields[0]]
  if (!prop) {
    return null
  }

  for (let i = 1; i < fields.length; i++) {
    const segment = fields[i]

    if (!prop) {
      return null
    }

    if (prop.type === 'text' && i === fields.length - 1) {
      return { type: 'string' }
    }

    if (prop.values) {
      // record types skip the next key
      prop = prop.values
    } else {
      if (!prop.properties) {
        return null
      }

      prop = prop.properties[segment]
    }
  }

  return prop
}

export const setNestedResult = (
  result: GetResult,
  field: string,
  value: any
) => {
  if (!field) {
    return
  }

  if (field === '') {
    for (const k in value) {
      result[k] = value[k]
    }

    return
  }

  const fields = field.split('.')
  const len = fields.length
  if (len > 1) {
    let segment = result
    for (let i = 0; i < len - 1; i++) {
      segment = segment[fields[i]] || (segment[fields[i]] = {})
    }
    segment[fields[len - 1]] = value
  } else {
    result[field] = value
  }
}
