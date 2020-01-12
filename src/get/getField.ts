import { SelvaClient } from '..'
import { Id, Language, languages, itemTypes } from '../schema'
import { GetResult, getInner } from './'

const setNestedResult = (result: GetResult, field: string, value: any) => {
  const fields = field.split('.')
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

const number = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult
): Promise<void> => {
  setNestedResult(result, field, 1 * (await client.redis.hget(id, field)))
}

const string = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult
): Promise<void> => {
  // string
  setNestedResult(result, field, (await client.redis.hget(id, field)) || '')
}

const set = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result?: GetResult
): Promise<void> => {
  setNestedResult(result, field, client.redis.smembers(id + '.' + field))
}

const stringified = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult
): Promise<void> => {
  const value = await client.redis.hget(id, field)
  setNestedResult(result, field, JSON.parse(value))
}

const object = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult
): Promise<void> => {
  // result.$keys is a cache
  if (!result.$keys) {
    result.$keys = await client.redis.hkeys(id)
  }

  // getInner

  //

  console.log('ok', result)
  // directly puts on result

  // return client.redis.hget(id, field)
}

const text = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language
): Promise<void> => {
  //   return 'lullz'
  if (!language) {
    await object(client, id, field, result)
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
  field: string,
  result: GetResult
): Promise<void> => {
  await object(client, id, field, result)
  result.auth.role.ids = await client.redis.smembers(id + '.auth.role.ids')
}

const id = async (
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult
) => {
  result.id = id
}

const types = {
  id: id,
  title: text,
  description: text,
  article: text,
  video: object, // stringified for overlayarray
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

async function getField(
  client: SelvaClient,
  id: Id,
  field: string,
  result: GetResult,
  language?: Language,
  version?: string
) {
  // think about version...
  const fn = types[field] || string
  await fn(client, id, field, result, language)
}

export default getField
