import { Filter } from '../types'
import { SelvaClient } from '../..'

import checkAllowed from './checkAllowed'

export default function validateFilter(
  client: SelvaClient,
  filter: Filter,
  path: string
): void {
  const err = (mainMsg?: string): never => {
    if (!mainMsg) {
      mainMsg = 'Unsupported type in operator $filter'
    }

    throw new Error(
      `${mainMsg} for filter in ${path}. Required type object with the following properties:
        {
          $operator: '=' | '!=' | '>' | '<' | '..'
          $field: string
          $value: string | number | boolean | (string | number | boolean)[]

          $and: Filter (chain more filters with and clause) (optional)
          $or: Filter (chain more filters with or clause) (optional)
        }
        
        or for geo filters

        {      
          $operator: 'distance'
          $field: string
          $value: {
            $lat: number
            $lon: number
            $radius: number
          }

          $and: Filter (chain more filters with and clause) (optional)
          $or: Filter (chain more filters with or clause) (optional)
        }

        or for exists filter

        {
          $operator: 'exists' | 'notExists'
          $field: string

          $and: Filter (chain more filters with and clause) (optional)
          $or: Filter (chain more filters with or clause) (optional)
        }
    `
    )
  }

  if (!filter.$field || typeof filter.$field !== 'string') {
    err(
      `Filter ${filter.$operator} should have a string field, got ${filter.$field}`
    )
  }

  if (
    filter.$operator !== '=' &&
    filter.$operator !== '!=' &&
    filter.$operator !== '>' &&
    filter.$operator !== '<' &&
    filter.$operator !== '..' &&
    filter.$operator !== 'distance' &&
    filter.$operator !== 'exists' &&
    filter.$operator !== 'notExists'
  ) {
    err(
      `Unsupported $operator ${filter.$operator}, has to be one of =, !=, >, <, .., distance, exists, notExists`
    )
  }

  if (filter.$operator === 'exists' || filter.$operator === 'notExists') {
    if (filter.$value) {
      err(`$value not allowed for filter type 'exists/notExists'`)
    }

    const allowed = checkAllowed(
      filter,
      new Set(['$operator', '$field', '$and', '$or'])
    )

    if (allowed !== true) {
      err(`Unsupported operator or field ${allowed}`)
    }
  } else if (filter.$operator === 'distance') {
    if (!filter.$value || typeof filter.$value !== 'object') {
      err(
        `$value of distance filter should be provided and should be an object with $lat, $lon and $radius'`
      )
    }

    if (!filter.$value.$lat || !filter.$value.$lon || !filter.$value.$radius) {
      err(
        `$value of distance filter should be provided and should be an object with $lat, $lon and $radius'`
      )
    }

    if (typeof filter.$value.$lat !== 'number') {
      err(
        `$value.$lat of distance filter should be provided and should be a number`
      )
    }

    if (typeof filter.$value.$lon !== 'number') {
      err(
        `$value.$lon of distance filter should be provided and should be a number`
      )
    }

    if (typeof filter.$value.$radius !== 'number') {
      err(
        `$value.$radius of distance filter should be provided and should be a number`
      )
    }

    const allowed = checkAllowed(
      filter,
      new Set(['$operator', '$field', '$value', '$and', '$or'])
    )

    if (allowed !== true) {
      err(`Unsupported operator or field ${allowed}`)
    }
  } else if (filter.$operator === '=' || filter.$operator === '!=') {
    if (
      filter.$value === null ||
      filter.$value === undefined ||
      (typeof filter.$value !== 'string' &&
        typeof filter.$value !== 'number' &&
        typeof filter.$value !== 'boolean' &&
        !Array.isArray(filter.$value))
    ) {
      err(
        `$value of ${filter.$operator} filter should be provided and should be a string, number, boolean or an array of them, got ${filter.$value}`
      )
    }
  } else if (filter.$operator === '<' || filter.$operator === '>') {
    if (
      !filter.$value ||
      (typeof filter.$value !== 'string' && typeof filter.$value !== 'number')
    ) {
      err(
        `$value of ${filter.$operator} filter should be provided and should be a string or number'`
      )
    }
  } else if (filter.$operator === '..') {
    if (
      !filter.$value ||
      (typeof filter.$value !== 'string' &&
        typeof filter.$value !== 'number' &&
        typeof filter.$value !== 'boolean' &&
        !Array.isArray(filter.$value))
    ) {
      err(
        `$value of ${filter.$operator} filter should be provided and should be a string, number, boolean or an array of them, got ${filter.$value}`
      )
    }
  }

  if (filter.$and) {
    validateFilter(client, filter.$and, path + '.$and')
  }

  if (filter.$or) {
    validateFilter(client, filter.$or, path + '.$or')
  }
}
