import { GetOptions } from '../types'
import { SelvaClient } from '../..'
import validateTopLevel, { ExtraQueries } from '.'

import checkAllowed from './checkAllowed'

export default async function validateField(
  extraQueries: ExtraQueries,
  client: SelvaClient,
  field: string | string[] | { path: string | string[]; value: GetOptions },
  path: string
): Promise<void> {
  if (typeof field === 'string') {
    return
  }

  if (typeof field === 'object') {
    if (Array.isArray(field)) {
      return
    }

    const allowed = checkAllowed(<GetOptions>field, new Set(['path', 'value']))
    if (allowed !== true) {
      throw new Error(
        `Unsupported option ${allowed} in operator $field for ${path}.$field`
      )
    }

    return await validateTopLevel(extraQueries, client, field.value, path)
  }

  throw new Error(
    `Unsupported type in operator $field for ${path}.$field. Required type string, array of strings or object { path: string | string[]; value: GetOptions }`
  )
}

