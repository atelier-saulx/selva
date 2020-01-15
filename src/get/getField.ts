import { SelvaClient } from '..'
import { Id, Language, languages, itemTypes, getTypeFromId } from '../schema'
import { GetResult, getInner, GetOptions, get } from './'
import { setNestedResult, getNestedField } from './nestedFields'
import { Verify } from 'crypto'

const number = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const v = await client.redis.hget(id, field)
  const value = v === null ? null : v * 1
  setNestedResult(result, field, value)
  return value !== null
}

const boolean = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult,
  language?: Language,
  version?: string
): Promise<true> => {
  setNestedResult(result, field, !!(await client.redis.hget(id, field)))
  return true
}

const string = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const value = (await client.redis.hget(id, field)) || ''
  setNestedResult(result, field, value)
  return !!value
}

const set = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const value = await client.redis.smembers(id + '.' + field)
  setNestedResult(result, field, value)
  return !!value.length
}

const stringified = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const value = await client.redis.hget(id, field)
  setNestedResult(result, field, value === null ? value : JSON.parse(value))
  return value !== null
}

const object = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<boolean> => {
  const keys = await client.redis.hkeys(id)
  let isComplete = true
  let noKeys = true
  for (const key of keys) {
    if (key.indexOf(field) === 0) {
      noKeys = false
      if (!(await getField(client, id, key, result, language))) {
        isComplete = false
      }
    }
  }
  return noKeys ? false : isComplete
}

const text = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<boolean> => {
  if (!language) {
    const isComplete = await object(
      client,
      id,
      field,
      result,
      language,
      version
    )
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
    const value = await client.redis.hget(id, field + '.' + language)
    if (value) {
      setNestedResult(result, field, value)
      return true
    } else {
      for (const lang of languages) {
        if (lang !== language) {
          const value = await client.redis.hget(id, field + '.' + lang)
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
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<true> => {
  const r = await object(client, id, field, result)
  result.auth.role.id = await client.redis.smembers(id + '.auth.role.id')
  return true
}

const id = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<true> => {
  result.id = id
  return true
}

const type = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<true> => {
  result.type = getTypeFromId(id)
  return true
}

const ancestors = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<true> => {
  result.ancestors = ((await client.redis.hget(id, field)) || '').split(',')
  return true
}

const getDescendants = async (
  client: SelvaClient,
  id: Id,
  results: Record<Id, true>,
  passedId: Record<Id, true>
): Promise<Record<Id, true>> => {
  if (!passedId[id]) {
    const children = await client.redis.smembers(id + '.children')
    for (const id of children) {
      results[id] = true
    }
    passedId[id] = true
    await Promise.all(
      children.map(c => getDescendants(client, c, results, passedId))
    )
  }
  return results
}

const descendants = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<true> => {
  const s = await getDescendants(client, id, {}, {})
  const r = []
  for (let key in s) {
    r.push(key)
  }
  result.descendants = r
  return true
}

type Reader = (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
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

async function getField(
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
): Promise<boolean> {
  // version still missing!
  const fn = types[field] || string
  return await fn(client, id, field, result, language, version)
}

export default getField
