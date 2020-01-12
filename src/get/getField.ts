import { SelvaClient } from '..'
import { Id, Language, languages, itemTypes, getTypeFromId } from '../schema'
import { GetResult, getInner, GetOptions, get } from './'
import setNestedResult from './setNestedResult'

type Props = GetOptions | true

const number = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result?: GetResult
): Promise<void> => {
  setNestedResult(result, field, 1 * (await client.redis.hget(id, field)))
}

const string = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result?: GetResult
): Promise<void> => {
  setNestedResult(result, field, (await client.redis.hget(id, field)) || '')
}

const set = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result?: GetResult
): Promise<void> => {
  setNestedResult(result, field, await client.redis.smembers(id + '.' + field))
}

const stringified = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result: GetResult
): Promise<void> => {
  const value = await client.redis.hget(id, field)
  setNestedResult(result, field, JSON.parse(value))
}

const object = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result: GetResult,
  language?: Language
): Promise<void> => {
  // super innefficient - memoize
  const keys = await client.redis.hkeys(id)
  if (props === true) {
    for (const key of keys) {
      if (key.indexOf(field) === 0) {
        await getField(client, id, true, key, result, language)
      }
    }
  } else {
    for (let key in props) {
      const nestedField = field + '.' + key
      if (props[key] === true) {
        await getField(client, id, true, nestedField, result, language)
      } else {
        await getInner(client, props[key], result, id, nestedField, language)
      }
    }
  }
}

const text = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result: GetResult,
  language?: Language
): Promise<void> => {
  if (!language) {
    await object(client, id, props, field, result)
  } else {
    const value = await client.redis.hget(id, field + '.' + language)
    if (value) {
      setNestedResult(result, field, value)
    } else {
      let found: boolean = false
      for (const lang of languages) {
        if (lang !== language) {
          const value = await client.redis.hget(id, field + '.' + lang)
          if (value) {
            found = true
            setNestedResult(result, field, value)
            break
          }
        }
      }
      if (found === false) {
        setNestedResult(result, field, '')
      }
    }
  }
}

const authObject = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result: GetResult
): Promise<void> => {
  await object(client, id, props, field, result)
  if (props === true) {
    result.auth.role.id = await client.redis.smembers(id + '.auth.role.id')
  }
}

const id = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result: GetResult
) => {
  result.id = id
}

const type = async (
  client: SelvaClient,
  id: Id,
  props: Props,
  field: string,
  result: GetResult
) => {
  // also never have to store this!
  result.type = getTypeFromId(id)
}

const types = {
  id: id,
  type: type,
  title: text,
  description: text,
  article: text,
  video: object, // stringified for overlayArray
  image: object,
  value: number,
  age: number,
  date: number,
  start: number,
  end: number,
  children: set,
  parents: set,
  auth: authObject,
  'auth.role': authObject,
  'auth.role.id': set,
  ancestors: async () => {},
  descendants: async () => {},
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
  props: Props,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
) {
  // think about version...
  const fn = types[field] || string
  await fn(client, id, props, field, result, language)
}

export default getField
