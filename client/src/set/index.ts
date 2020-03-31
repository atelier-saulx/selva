import { SetOptions } from './types'
import { SelvaClient } from '..'
import { Schema } from '../schema'
import fieldParsers from './fieldParsers'
import { verifiers } from './fieldParsers/simple'
import { configureLogger } from 'lua/src/logger'

// import { MAX_BATCH_SIZE } from '../redis'
const MAX_BATCH_SIZE = 5

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
      } else if (key === '$_itemCount') {
        // ignore
        result[key] = payload[key]
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

  if (parsed.$_itemCount > MAX_BATCH_SIZE) {
    const [id] = await setInBatches(client, parsed)
    return id
  }

  // need to check for error of schema
  const modifyResult = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>parsed // assure TS that id is actually set :|
  })

  return <string>modifyResult
}

async function setInBatches(
  client: SelvaClient,
  payload: SetOptions | SetOptions[]
): Promise<string[]> {
  if (Array.isArray(payload)) {
    // const BATCH_SIZE = MAX_BATCH_SIZE - 10 // allow for some operations to be batched with these
    const BATCH_SIZE = MAX_BATCH_SIZE
    const allIds: string[] = []
    let idx = 0

    do {
      const slice = payload.slice(idx, idx + BATCH_SIZE)

      const ids = await Promise.all(
        slice.map(i => {
          return client.modify({
            kind: 'update',
            payload: <SetOptions & { $id: string }>i // assure TS that id is actually set :|
          })
        })
      )

      allIds.push(...(<string[]>ids))
      idx += BATCH_SIZE
    } while (idx < payload.length)

    return allIds
  }

  if (payload.$_itemCount < MAX_BATCH_SIZE && payload.$_itemCount !== 0) {
    const id = await client.modify({
      kind: 'update',
      payload: <SetOptions & { $id: string }>payload // assure TS that id is actually set :|
    })

    payload.$id = <string>id
    payload.$_itemCount = 0
    return [<string>id]
  }

  const size = payload.$_itemCount
  let fieldNames: string[] = []
  let missingFieldNames: string[] = []
  let processedFields: { [x: string]: string[] | { $add: string[] } } = {}
  let batchQueue: SetOptions[] = []
  let remainingBatchSize = MAX_BATCH_SIZE
  for (const field in payload) {
    if (payload[field].$_itemCount) {
      if (remainingBatchSize - payload[field].$_itemCount >= 0) {
        remainingBatchSize -= payload[field].$_itemCount
        batchQueue.push(payload[field])
        fieldNames.push(field)
      } else if (payload[field].$_itemCount > MAX_BATCH_SIZE) {
        if (payload[field].$add) {
          const refIds = await setInBatches(client, payload[field].$add)
          processedFields[field] = { $add: refIds }
        } else if (Array.isArray(payload[field])) {
          const refIds = await setInBatches(client, payload[field])
          processedFields[field] = refIds
        } else {
          fieldNames.push(field)
        }
      } else {
        missingFieldNames.push(field)
      }
    } else {
      fieldNames.push(field)
    }
  }

  const newPayload: SetOptions = {}
  for (const field of fieldNames) {
    newPayload[field] = payload[field]
  }

  for (const field in processedFields) {
    newPayload[field] = processedFields[field]
  }

  newPayload.$_itemCount = MAX_BATCH_SIZE - remainingBatchSize
  const id = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>newPayload // assure TS that id is actually set :|
  })

  const missingPayload: SetOptions = { $id: <string>id }
  let missingFieldsCount = 0
  for (const field of missingFieldNames) {
    missingFieldsCount++
    missingPayload[field] = payload[field]
  }

  if (missingFieldsCount) {
    missingPayload.$_itemCount = size - newPayload.$_itemCount
    await setInBatches(client, missingPayload)
  }

  return [<string>id]
}

export { set, SetOptions }
