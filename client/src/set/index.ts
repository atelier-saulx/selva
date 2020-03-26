import { SetOptions } from './types'
import { SelvaClient } from '..'
import { Schema } from '../schema'
import fieldParsers from './fieldParsers'
import { verifiers } from './fieldParsers/simple'
import { configureLogger } from 'lua/src/logger'

import { MAX_BATCH_SIZE } from '../redis'

export const parseSetObject = (
  payload: SetOptions,
  schemas: Schema,
  $lang?: string
): SetOptions => {
  const result: SetOptions = {}

  if (!payload.type && schemas.prefixToTypeMapping && payload.$id) {
    payload.type = schemas.prefixToTypeMapping[payload.$id.substring(0, 2)]
  }

  if (!payload.type && payload.$id === 'root') {
    payload.type = 'root'
  }

  if (payload.$language) {
    $lang = payload.$language
  }
  const type = payload.type

  const schema = type === 'root' ? schemas.rootType : schemas.types[type]
  if (!schema) {
    throw new Error(
      `Cannot find type ${type} from set-object ${JSON.stringify(
        payload,
        null,
        2
      )}`
    )
  }

  let fields = schema.fields
  for (let key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        if (!(payload[key] === true || payload[key] === false)) {
          throw new Error(`$merge needs to be a a boolean `)
        }
        result[key] = payload[key]
      } else if (key === '$id') {
        if (!verifiers.id(payload[key])) {
          throw new Error('Wrong type for $id ' + payload[key])
        }
        result[key] = payload[key]
      } else if (key === '$source') {
        if (
          typeof payload[key] !== 'string' &&
          typeof payload[key] !== 'object'
        ) {
          throw new Error('Wrong type for $source, string or object required')
        }

        result[key] = payload[key]
      } else if (key === '$alias') {
        if (typeof payload[key] !== 'string' && !Array.isArray(payload[key])) {
          throw new Error('Wrong type for $alias, string or array required')
        }

        result[key] = payload[key]
      } else if (key === '$version') {
        if (typeof payload[key] !== 'string') {
          throw new Error('Wrong type for $version')
        }
        console.warn('$version is not implemented yet!')
        result[key] = payload[key]
      } else if (key === '$language') {
        if (
          typeof payload[key] !== 'string' ||
          String(payload[key]).length > 100
        ) {
          throw new Error(`Wrong type for language ${payload[key]}`)
        }
      } else {
        throw new Error(`Wrong option on set object ${key}`)
      }
    } else if (!fields[key]) {
      throw new Error(`Cannot find field ${key} in ${type} from set-object`)
    } else {
      const fn = fieldParsers[fields[key].type]
      fn(schemas, key, payload[key], result, fields[key], type, $lang)
    }
  }
  return result
}

async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  let gotSchema = false
  if (!client.schema) {
    await client.getSchema()
    gotSchema = true
  }
  // need to check if it updated

  let schema = client.schema

  let parsed
  try {
    parsed = parseSetObject(payload, schema)
  } catch (err) {
    if (!gotSchema) {
      delete client.schema
      return set(client, payload)
    } else {
      throw err
    }
  }

  console.log('PARSED', parsed)
  const batches = []
  if (parsed.$_itemCount > MAX_BATCH_SIZE) {
  }

  // need to check for error of schema
  const modifyResult = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>parsed // assure TS that id is actually set :|
  })

  console.log(modifyResult)

  return <string>modifyResult
}

async function setInBatches(
  client: SelvaClient,
  payload: SetOptions,
  remainingBatchSize: number
): Promise<string> {
  if (payload.$_itemCount <= MAX_BATCH_SIZE && payload.$_itemCount !== 0) {
    const id = await set(client, payload)
    payload.$id = id
    payload.$_itemCount = 0
    return id
  }

  const withoutRefs = {}
  const refs = {}
  for (const field in payload) {
    if (payload[field].$_itemCount) {
      let items: SetOptions[]
      if (Array.isArray(payload[field])) {
        items = payload[field]
      } else {
        items = payload[field].$add
        refs[field].$_add = true
      }

      const stringIds: string[] = []
      for (const item of items) {
        if (typeof item === 'object') {
          const id = await setInBatches(client, item, remainingBatchSize)
          stringIds.push(id)
          // FIXME: we'll definitely want to set multiple indices in references at once if they can fit
        } else {
          stringIds.push(item)
        }
      }

      refs[field] = stringIds
    } else {
      withoutRefs[field] = payload[field]
    }
  }

  const refsPayload = {}
  for (const field in refs) {
    if (refs[field].$_add) {
      refsPayload[field] = { $add: refs[field] }
    } else {
      refsPayload[field] = refs[field]
    }
  }

  await set(client, { ...withoutRefs, ...refsPayload })
}

export { set, SetOptions }
