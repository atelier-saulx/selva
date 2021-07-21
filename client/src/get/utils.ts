import { FieldSchema, Schema } from '../schema'
import { GetResult } from './types'

export const getNestedField = (result: GetResult, field: string): any => {
  if (!field || field === '') {
    return result
  }

  if (!result) {
    return undefined
  }

  const fields = field.split('.')
  const len = fields.length
  if (len > 1) {
    let segment = result
    for (let i = 0; i < len; i++) {
      segment = segment[fields[i]]
      if (segment === undefined) {
        return undefined
      }
    }
    return segment
  } else {
    return result[field]
  }
}

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

  let firstSegment = fields[0]
  if (firstSegment.endsWith(']')) {
    // sanitize array types to look up the array so it ends up in the array object schema if below
    for (let j = firstSegment.length - 1; j >= 0; j--) {
      if (firstSegment[j] === '[') {
        firstSegment = firstSegment.slice(0, j)
        break
      }
    }
  }

  let prop: any = typeSchema.fields[firstSegment]
  if (!prop) {
    return null
  }

  for (let i = 1; i < fields.length; i++) {
    let segment = fields[i]
    console.log('heyoo', id, field, segment, JSON.stringify(prop, null, 2))

    if (segment.endsWith(']')) {
      // sanitize array types to look up the array so it ends up in the array object schema if below
      for (let j = segment.length - 1; j >= 0; j--) {
        if (segment[j] === '[') {
          segment = segment.slice(0, j)
          break
        }
      }
    }

    if (!prop) {
      return null
    }

    if (prop.type === 'text' && i === fields.length - 1) {
      return { type: 'string' }
    }

    if (prop.values) {
      if (i < fields.length - 1) {
        // record types skip the next key
        prop = prop.values
      }
    } else if (prop.type === 'array') {
      prop = prop.items
      prop = prop.properties[segment]
    } else {
      if (!prop.properties) {
        return null
      }

      prop = prop.properties[segment]
    }
  }

  return prop
}

export const padId = (id: string): string => {
  return id.padEnd(10, '\0')
}

export const setNestedResult = (
  result: GetResult,
  field: string,
  value: any
) => {
  if (field === null || field === undefined) {
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

export const joinIds = (ids: string[]): string =>
  ids.map((id) => padId(id)).join('')
