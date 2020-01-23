import { SetOptions } from './types'
import { SelvaClient } from '..'
import { TypeSchema } from '../schema'
import collectSchemas from './collectSchemas'
import fieldParsers from './fieldParsers'
import { verifiers } from './fieldParsers/simple'

export const parseSetObject = (
  payload: SetOptions,
  schemas: Record<string, TypeSchema>
): SetOptions => {
  const result: SetOptions = {}
  const type = payload.type
  const schema = schemas[type]
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
  let schemas
  try {
    schemas = await collectSchemas(client, payload, {})
  } catch (err) {
    throw err
  }
  const parsed = parseSetObject(payload, schemas)
  const modifyResult = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>parsed // assure TS that id is actually set :|
  })

  return modifyResult[0]
}

export { set, SetOptions }
