import { SetOptions } from './types'
import { SelvaClient } from '..'
import { Types, Schema } from '../schema'
import fieldParsers from './fieldParsers'
import { verifiers } from './fieldParsers/simple'

export const parseSetObject = (
  payload: SetOptions,
  schemas: Schema
): SetOptions => {
  const result: SetOptions = {}
  const type = payload.type
  const schema = schemas.types[type]
  if (!schema) {
    throw new Error(`Cannot find type ${type} from set-object`)
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
      } else if (key === '$version') {
        if (typeof payload[key] !== 'string') {
          throw new Error('Wrong type for $version')
        }
        console.warn('$version is not implemented yet!')
        result[key] = payload[key]
      } else {
        throw new Error(`Wrong option on set object ${key}`)
      }
    } else if (!fields[key]) {
      throw new Error(`Cannot find field ${key} in ${type} from set-object`)
    } else {
      const fn = fieldParsers[fields[key].type]
      fn(schemas, key, payload[key], result, fields[key], type)
    }
  }
  return result
}

async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  if (!client.schema) {
    await client.getSchema()
  }

  let schema = client.schema
  console.log('!!schemas', schema)
  console.log('!!!payload', payload)
  if (schema.prefixToTypeMapping && payload.$id && !payload.type) {
    payload.type = schema.prefixToTypeMapping[payload.$id.substring(0, 2)]
  }

  const parsed = parseSetObject(payload, schema)
  console.log(`sending parsed ${JSON.stringify(parsed)}`)
  const modifyResult = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>parsed // assure TS that id is actually set :|
  })

  return modifyResult[0]
}

export { set, SetOptions }
