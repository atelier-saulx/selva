import { SetOptions } from './types'
import { SelvaClient } from '..'
import { TypeSchema } from '../schema'
import collectSchemas from './collectSchemas'
import fieldParsers from './fieldParsers'

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
      result[key] = payload[key]
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
  // console.log('result', JSON.stringify(parsed, void 0, 2))
  const modifyResult = await client.modify({
    kind: 'update',
    payload: <SetOptions & { $id: string }>payload // assure TS that id is actually set :|
  })

  // return modifyResult[0]
  return 'ok'
}

export { set, SetOptions }
