import { GetResult } from '~selva/get/types'
import { splitString } from '../util'
import * as logger from '../logger'

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
