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
