import { SelvaClient } from '..'
import { Id, Language, languages, itemTypes, getTypeFromId } from '../schema'
import { GetResult, getInner, GetOptions, get } from './'
import { setNestedResult } from './nestedFields'

const number = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult,
  language?: Language
): Promise<void> => {
  const v = await client.redis.hget(id, field)
  setNestedResult(result, field, v === null ? null : v * 1)
}

const boolean = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult,
  language?: Language
): Promise<void> => {
  setNestedResult(result, field, !!(await client.redis.hget(id, field)))
}

const string = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult,
  language?: Language
): Promise<void> => {
  return setNestedResult(
    result,
    field,
    (await client.redis.hget(id, field)) || ''
  )
}

const set = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult,
  language?: Language
): Promise<void> => {
  return setNestedResult(
    result,
    field,
    await client.redis.smembers(id + '.' + field)
  )
}

const stringified = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
): Promise<void> => {
  const value = await client.redis.hget(id, field)
  return setNestedResult(
    result,
    field,
    value === null ? value : JSON.parse(value)
  )
}

const object = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
): Promise<void> => {
  // super innefficient - memoize
  const keys = await client.redis.hkeys(id)
  for (const key of keys) {
    if (key.indexOf(field) === 0) {
      await getField(client, id, key, result, language)
    }
  }
}

const text = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
): Promise<void> => {
  if (!language) {
    return await object(client, id, field, result)
  } else {
    const value = await client.redis.hget(id, field + '.' + language)
    if (value) {
      setNestedResult(result, field, value)
    } else {
      for (const lang of languages) {
        if (lang !== language) {
          const value = await client.redis.hget(id, field + '.' + lang)
          if (value) {
            setNestedResult(result, field, value)
          }
        }
      }
      setNestedResult(result, field, '')
    }
  }
}

const authObject = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
): Promise<void> => {
  const r = await object(client, id, field, result)
  result.auth.role.id = await client.redis.smembers(id + '.auth.role.id')
}

const id = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
) => {
  result.id = id
}

const type = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
) => {
  // also never have to store this!
  result.type = getTypeFromId(id)
}

const ancestors = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
) => {
  result.ancestors = ((await client.redis.hget(id, field)) || '').split(',')
}

const getDescendants = async (
  client: SelvaClient,
  id: Id,
  passedId: Record<Id, true>
): Promise<Id[]> => {
  if (!passedId[id]) {
    const children = await client.redis.smembers(id + '.children')
    const results = []
    passedId[id] = true
    ;(
      await Promise.all(children.map(c => getDescendants(client, c, passedId)))
    ).forEach(v => results.push(...v))
    return results
  } else {
    return []
  }
}

const descendants = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
) => {
  result.descendants = await getDescendants(client, id, {})
}

type Reader = (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
) => Promise<any>

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

// test to see if we are missing things here (read from ts)
async function getField(
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
) {
  // version still missing!
  const fn = types[field] || string
  await fn(client, id, field, result, language)
}

export default getField
