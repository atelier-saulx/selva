import {
  Id,
  Language,
  languages,
  itemTypes,
  getTypeFromId
} from '~selva/schema'
import * as redis from '../redis'
import { GetResult } from '~selva/get/types'
import { setNestedResult, getNestedField } from './nestedFields'

const number = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const v = redis.hget(id, field)
  const value = v === null ? null : v * 1
  setNestedResult(result, field, value)
  return value !== null
}

const boolean = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<true> => {
  setNestedResult(result, field, !!redis.hget(id, field))
  return true
}

const string = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const value = redis.hget(id, field) || ''
  setNestedResult(result, field, value)
  return !!value
}

const set = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const value = redis.smembers(id + '.' + field)
  setNestedResult(result, field, value)
  return !!value.length
}

const stringified = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const value = redis.hget(id, field)
  setNestedResult(result, field, value === null ? value : JSON.parse(value))
  return value !== null
}

const object = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const keys = redis.hkeys(id)
  let isComplete = true
  let noKeys = true
  for (const key of keys) {
    if (key.indexOf(field) === 0) {
      noKeys = false
      if (getByType(result, id, key, language)) {
        isComplete = false
      }
    }
  }
  return noKeys ? false : isComplete
}

const text = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<boolean> => {
  if (!language) {
    const isComplete = await object(result, id, field, language, version)
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
      for (const lang of languages) {
        if (lang !== language) {
          const value = redis.hget(id, field + '.' + lang)
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

const authObject = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<true> => {
  const r = object(result, id, field)
  result.auth.role.id = redis.smembers(id + '.auth.role.id')
  return true
}

const id = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<true> => {
  result.id = id
  return true
}

const type = async (
  result: GetResult | null,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<true> => {
  result.type = getTypeFromId(id)
  return true
}

const ancestors = async (
  result: GetResult,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<true> => {
  // result.ancestors = (redis.hget(id, field) || '').split(',')
  result.ancestors = redis.hget(id, field) || ''
  return true
}

const getDescendants = async (
  id: Id,
  results: Record<Id, true>,
  passedId: Record<Id, true>
): Promise<Record<Id, true>> => {
  if (!passedId[id]) {
    const children = redis.smembers(id + '.children')
    for (const id of children) {
      results[id] = true
    }
    passedId[id] = true
    await Promise.all(children.map(c => getDescendants(c, results, passedId)))
  }
  return results
}

const descendants = async (
  result: GetResult,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<true> => {
  const s = await getDescendants(id, {}, {})
  const r = []
  for (let key in s) {
    r.push(key)
  }
  result.descendants = r
  return true
}

type Reader = (
  result: GetResult,
  id: Id,
  field: string,
  language?: Language,
  version?: string
) => Promise<boolean>

const types: Record<string, Reader> = {
  id: id,
  type: type,
  title: text,
  description: text,
  article: text,
  video: object, // stringified for overlayArray
  image: object,
  contact: object,
  'contact.phone': number,
  value: number,
  age: number,
  status: number,
  date: number,
  start: number,
  price: number,
  discount: number,
  tax: number,
  end: number,
  published: boolean,
  children: set,
  parents: set,
  auth: authObject,
  'auth.role': authObject,
  'auth.role.id': set,
  ancestors,
  descendants,
  layout: object,
  'layout.default': stringified
}

for (const type of itemTypes) {
  types['layout.' + type] = stringified
}

async function getByType(
  result: GetResult,
  id: Id,
  field: string,
  language?: Language,
  version?: string
): Promise<boolean> {
  // version still missing!
  const fn = types[field] || string
  return await fn(result, id, field, language, version)
}

export default getByType
