import { SetOptions } from './types'
import { SelvaClient } from '..'
import { Schema } from '../schema'
import fieldParsers from './fieldParsers'
import { verifiers } from './fieldParsers/simple'

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
      } else if (key === '$alias') {
        if (
          typeof payload[key] !== 'string' &&
          typeof payload[key] !== 'object'
        ) {
          throw new Error('Wrong type for $alias, string required')
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
  if (!client.schema) {
    await client.getSchema()
  }

  let schema = client.schema

  const parsed = parseSetObject(payload, schema)
  const modifyResult = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>parsed // assure TS that id is actually set :|
  })

  return <string>modifyResult
}

export { set, SetOptions }
