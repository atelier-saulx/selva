import { Sort } from '../types'
import { SelvaClient } from '../..'

import checkAllowed from './checkAllowed'

export default function validateSort(
  client: SelvaClient,
  sort: Sort,
  path: string
): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $sort'
    }

    throw new Error(
      `${mainMsg} for ${path}.$sort. Required type object with the following properties:
        {
          $field: string
          $order: 'asc' | 'desc' (optional)
        }
    `
    )
  }

  const allowed = checkAllowed(sort, new Set(['$field', '$order']))
  if (allowed !== true) {
    err(`Unsupported operator or field ${allowed}`)
  }

  if (!sort.$field || typeof sort.$field !== 'string') {
    err(`Unsupported type of operator $field with value ${sort.$field}`)
  }

  if (sort.$order) {
    const order = sort.$order.toLowerCase()
    if (order !== 'asc' && order !== 'desc') {
      err(`Unsupported sort order ${sort.$order}, 'asc'|'desc' required`)
    }
  }
}
