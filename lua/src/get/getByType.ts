import { Id } from '~selva/schema/index'

import * as redis from '../redis'
import { getTypeFromId } from '../typeIdMapping'
import { GetResult } from '~selva/get/types'
import { setNestedResult, getNestedField } from './nestedFields'
import { TypeSchema } from '../../../src/schema/index'

const id = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  result.id = id
  return true
}

const number = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  const v = redis.hget(id, field)
  const value = v === null ? null : tonumber(v)
  setNestedResult(result, field, value)
  return value !== null
}

const float = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  const v = redis.hget(id, field)
  const value = v === null ? null : Math.floor(tonumber(v))
  setNestedResult(result, field, value)
  return value !== null
}

const int = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  return number(result, schemas, id, field, language, version)
}

const boolean = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): true => {
  setNestedResult(result, field, !!redis.hget(id, field))
  return true
}

const string = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  const value = redis.hget(id, field) || ''
  setNestedResult(result, field, value)
  return !!value
}

const arrayLike = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  const value = redis.smembers(id + '.' + field)
  setNestedResult(result, field, value)
  return !!value.length
}

const json = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  const value = redis.hget(id, field)
  setNestedResult(result, field, value === null ? value : JSON.parse(value))
  return value !== null
}

const object = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  const keys = redis.hkeys(id)
  let isComplete = true
  let noKeys = true
  for (const key of keys) {
    if (key.indexOf(field) === 0) {
      noKeys = false
      if (getByType(result, schemas, id, `${field}.${key}`, language)) {
        isComplete = false
      }
    }
  }
  return noKeys ? false : isComplete
}

const text = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  if (!language) {
    const isComplete = object(result, schemas, id, field, language, version)
    if (!isComplete) {
      const value = getNestedField(result, field)
      for (const key in value) {
        if (value[key]) {
          return true
        }
      }
      return false
    } else {
      return true
    }
  } else {
    const value = redis.hget(id, field + '.' + language)
    if (value) {
      setNestedResult(result, field, value)
      return true
    } else {
      // for (const lang of languages) {
      //   if (lang !== language) {
      //     const value = redis.hget(id, field + '.' + lang)
      //     if (value) {
      //       setNestedResult(result, field, value)
      //       return true
      //     }
      //   }
      // }
      setNestedResult(result, field, '')
      return false
    }
  }
}

const ancestors = (
  result: GetResult,
  id: Id,
  field: string,
  language?: string,
  version?: string
): true => {
  // result.ancestors = (redis.hget(id, field) || '').split(',')
  result.ancestors = redis.hget(id, field) || ''
  return true
}

const getDescendants = (
  id: Id,
  results: Record<Id, true>,
  passedId: Record<Id, true>
): Record<Id, true> => {
  if (!passedId[id]) {
    const children = redis.smembers(id + '.children')
    for (const id of children) {
      results[id] = true
    }
    passedId[id] = true
    for (const c of children) {
      getDescendants(c, results, passedId)
    }
  }
  return results
}

const descendants = (
  result: GetResult,
  id: Id,
  field: string,
  language?: string,
  version?: string
): true => {
  const s = getDescendants(id, {}, {})
  const r: string[] = []
  let idx = 0
  for (let key in s) {
    r[idx] = key
    idx++
  }
  result.descendants = r
  return true
}

const types = {
  id,
  string,
  digest: string,
  number,
  float,
  int,
  boolean,
  set: arrayLike,
  references: arrayLike,
  json,
  array: json,
  text,
  timestamp: number,
  url: string,
  email: string,
  phone: string,
  geo: object,
  type: string,
  descendants,
  ancestors
}

function getByType(
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean {
  // version still missing!

  const type = getTypeFromId(id)
  const schema = schemas[type]
  if (!schema || !schema.fields) {
    return true
  }

  const paths = field.split('.')
  let prop = schema.fields[paths[0]]
  for (let i = 1; i < paths.length; i++) {
    if (!prop || prop.type !== 'object') {
      return true
    }

    prop = prop.properties[paths[i]]
  }

  if (!prop) {
    return true
  }

  const fn = types[prop.type] || string
  return fn(result, schemas, id, field, language, version)
}

export default getByType
