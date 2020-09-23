import { GetResult } from '~selva/get/types'
import { splitString, ensureArray } from '../util'
import * as logger from '../logger'

import globals from '../globals'
import { getSchema } from '../schema/index'
import { FieldSchema } from '~selva/schema/index'
import { getTypeFromId } from '../typeIdMapping'

const SCHEMA_PATH_CACHE: Record<string, Record<string, FieldSchema>> = {}

export const getNestedField = (result: GetResult, field: string): any => {
  if (!field || field === '') {
    return result
  }

  const fields = splitString(field, '.')
  const len = fields.length
  if (len > 1) {
    let segment = result
    for (let i = 0; i < len; i++) {
      segment = segment[fields[i]]
      if (segment === null) {
        return null
      }
    }
    return segment
  } else {
    return result[field]
  }
}

export function getNestedSchema(id: string, field: string): FieldSchema | null {
  if (!field || field === '') {
    return null
  }

  const type = getTypeFromId(id)
  const schema = getSchema()

  let typeCache = SCHEMA_PATH_CACHE[type]

  if (!typeCache) {
    typeCache = {}
    SCHEMA_PATH_CACHE[type] = typeCache
  }

  if (typeCache[field]) {
    return typeCache[field]
  }

  const fields = splitString(field, '.')

  const typeSchema = type === 'root' ? schema.rootType : schema.types[type]
  if (!typeSchema || !typeSchema.fields) {
    return null
  }

  let prop: any = typeSchema.fields[fields[0]]
  if (!prop) {
    return null
  }

  let str = fields[0]
  for (let i = 1; i < fields.length; i++) {
    const segment = fields[i]

    if (!prop) {
      return null
    }

    if (prop.values) {
      // record types skip the next key
      prop = prop.values
    } else {
      if (!prop.properties) {
        return null
      }

      prop = prop.properties[segment]

      str += '.' + segment
      typeCache[str] = prop
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

  const fields = splitString(field, '.')
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

export function setMeta(
  fields?: string | string[],
  fieldOpts?: {
    ___ids?: string | string[]
    ___types?: Record<string, Record<string, true>> // type => queryid
    ___any?: string | string[] // query id
  },
  globalOpts?: {
    ___refreshAt?: number
    ___contains?: Record<string, { $field: string; $value: string[] }>
  }
): void {
  if (!globals.$meta) {
    return
  }

  if (fields && fieldOpts) {
    for (const field of ensureArray(fields)) {
      const current = getNestedField(globals.$meta, field) || {}

      if (fieldOpts.___ids) {
        if (!current.___ids) {
          current.___ids = {}
        }

        for (const id of ensureArray(fieldOpts.___ids)) {
          current.___ids[id] = true
        }
      }

      if (fieldOpts.___types) {
        if (!current.___types) {
          current.___types = {}
        }

        for (const type in fieldOpts.___types) {
          if (!current.___types[type]) {
            current.___types[type] = {}
          }

          for (const contains in fieldOpts.___types[type]) {
            current.___types[type][contains] =
              fieldOpts.___types[type][contains]
          }
        }
      }

      if (fieldOpts.___any) {
        if (!current.___any) {
          current.___any = {}
        }

        for (const queryId of fieldOpts.___any) {
          current.___any[queryId] = true
        }
      }

      setNestedResult(globals.$meta, field, current)
    }
  }

  if (globalOpts) {
    if (globalOpts.___refreshAt) {
      if (globals.$meta.___refreshAt) {
        if (globalOpts.___refreshAt < globals.$meta.___refreshAt) {
          globals.$meta.___refreshAt = globalOpts.___refreshAt
        }
      } else {
        globals.$meta.___refreshAt = globalOpts.___refreshAt
      }
    }

    if (globalOpts.___contains) {
      const contains = globals.$meta.___contains || {}

      for (const queryId in globalOpts.___contains) {
        contains[queryId] = globalOpts.___contains[queryId]
      }

      globals.$meta.___contains = contains
    }
  }
}
