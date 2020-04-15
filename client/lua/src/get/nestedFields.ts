import { GetResult } from '~selva/get/types'
import { splitString, ensureArray } from '../util'
import * as logger from '../logger'

import globals from '../globals'

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

export const setNestedResult = (
  result: GetResult,
  field: string,
  value: any
) => {
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
    ___types?: Record<string, string | string[]> // type => queryid
    ___any?: string | string[] // query id
  },
  globalOpts?: {
    ___refreshAt?: number
    ___contains?: Record<string, { $field: string; $value: string }>
  }
): void {
  if (!globals.$meta) {
    return
  }

  if (fields && fieldOpts) {
    for (const field of ensureArray(fields)) {
      const current = getNestedField(globals.$meta, field) || {
        ___ids: {},
        ___types: {},
        ___any: {}
      }

      if (fieldOpts.___ids) {
        for (const id of ensureArray(fieldOpts.___ids)) {
          current.___ids[id] = true
        }
      }

      if (fieldOpts.___types) {
        for (const type in fieldOpts.___types) {
          for (const queryId of ensureArray(fieldOpts.___types[type])) {
            current.___types[type] = { [queryId]: true }
          }
        }
      }

      if (fieldOpts.___any) {
        for (const queryId of fieldOpts.___any) {
          current.___any[queryId] = true
        }
      }

      setNestedResult(globals.$meta, field, current)
    }

    if (globalOpts) {
      if (globalOpts.___refreshAt) {
        globals.$meta.___refreshAt = globalOpts.___refreshAt
      }

      if (globalOpts.___contains) {
        for (const queryId in globalOpts.___contains) {
          globals.$meta[queryId] = globalOpts.___contains[queryId]
        }
      }
    }
  }
}
