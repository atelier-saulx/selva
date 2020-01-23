import { Id } from '~selva/schema/index'

import * as redis from '../redis'
import { getTypeFromId } from '../typeIdMapping'
import { GetResult } from '~selva/get/types'
import { setNestedResult, getNestedField } from './nestedFields'
import { TypeSchema } from '../../../src/schema/index'
import { splitString, stringStartsWith, joinString } from '../util'
import * as logger from '../logger'

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
  return value !== null && value.length > 0
}

const arrayLike = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): boolean => {
  if (field === 'ancestors') {
    return ancestors(result, schemas, id, field, language, version)
  } else if (field === 'descendants') {
    return descendants(result, id, field, language, version)
  }

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
  setNestedResult(
    result,
    field,
    type(value) === 'string' ? cjson.decode(value) : null
  )
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
      if (!getByType(result, schemas, id, key, language)) {
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
      if (!value) {
        return false
      }

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
      const keys = redis.hkeys(id)
      for (let i = 0; i < keys.length; i++) {
        if (stringStartsWith(keys[i], field + '.')) {
          const value = redis.hget(id, keys[i])
          if (value) {
            setNestedResult(result, field, value)
            return true
          }
        }
      }

      setNestedResult(result, field, '')
      return false
    }
  }
}

const ancestors = (
  result: GetResult,
  schemas: Record<string, TypeSchema>,
  id: Id,
  field: string,
  language?: string,
  version?: string
): true => {
  result.ancestors = splitString(redis.hget(id, field) || '', ',')
  // result.ancestors = redis.hget(id, field) || ''
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
  object,
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
  logger.info(
    `getting field with id ${id} for field ${field} from type ${type}`
  )
  const schema = schemas[type]
  if (!schema || !schema.fields) {
    logger.info(`No schema for type ${type}`)
    return true
  }

  const paths = splitString(field, '.')
  let prop = schema.fields[paths[0]]
  for (let i = 1; i < paths.length; i++) {
    if (prop && prop.type === 'text' && i === paths.length - 1) {
      prop = { type: 'string' }
    } else if (prop && prop.type === 'json') {
      const json = types.json
      const intermediateResult = {}
      const pathToJson: string[] = []
      for (let j = 0; j < i; j++) {
        pathToJson[j] = paths[j]
      }

      json(
        intermediateResult,
        schemas,
        id,
        joinString(pathToJson, '.'),
        language,
        version
      )
      setNestedResult(result, field, getNestedField(intermediateResult, field))
      return true
    } else {
      if (!prop || prop.type !== 'object') {
        return false
      }

      prop = prop.properties[paths[i]]
    }
  }

  if (!prop) {
    logger.info(`No type for field ${field} in schema ${cjson.encode(schema)}`)
    return true
  }

  logger.info(`GETTING FIELD ${field} WITH TYPE ${prop.type}`)

  const fn = types[prop.type] || string
  return fn(result, schemas, id, field, language, version)
}

export default getByType
