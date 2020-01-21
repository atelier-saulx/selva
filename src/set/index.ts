import { SetOptions } from './types'
import { SelvaClient } from '..'
import getTypeFromId, { getPrefixes } from '../getTypeFromId'
import { Schema } from '../schema'
// verification stuff lets go
// for nested stuff need to get tpye schemas - shitty scince now this is all async bullshit

/*
  'float'
  'number'
  'int'
  'json'
  'array'
  'references'
  'set'
  'string'
  'object'
  'text'
  'id'
  'digest'
  'timestamp'
  'url'
  'email'
  'phone'
  'geo'
  'type'
*/

// nested field thing

const fieldTypes = {
  url: async (
    client: SelvaClient,
    fieldPayload: SetOptions,
    schemas: Record<string, Schema> = {},
    field: string[],
    parsed: SetOptions,
    prefixes?: Record<string, string>
  ): Promise<void> => {
    // ---
  }
}

const parseSetObject = async (
  client: SelvaClient,
  payload: SetOptions,
  schemas: Record<string, Schema> = {},
  prefixes?: Record<string, string>
): Promise<SetOptions> => {
  const result: SetOptions = {}
  console.log('Do it payload', result)

  if (!payload.type) {
    prefixes = await getPrefixes(client)
    result.type = await getTypeFromId(client, payload.$id, prefixes)
  } else {
    result.type = payload.type
  }

  // keep schemas in memmory
  const schema =
    schemas[result.type] ||
    (schemas[result.type] = JSON.parse(
      await client.redis.hget('types', result.type)
    ))

  if (!schema) {
    throw new Error(`No schema defined for ${result.type}`)
  }

  // console.log('--->', result.type, schema)

  // promise all for the queues --- :(

  for (const key in payload) {
    console.log(' -- KEY', key)

    if (schema.fields[key]) {
      // type map
    } else {
      throw new Error(`Field is not defined on type ${result.type} || ${key}`)
    }
  }

  // add prefix in here
  return result
}

// ---------------------------------------------------------------
async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  const parsed = await parseSetObject(client, payload)

  console.log(parsed)

  // const modifyResult = await client.modify({
  //   kind: 'update',
  //   payload: <SetOptions & { $id: string }>payload // assure TS that id is actually set :|
  // })

  // return modifyResult[0]
  return 'ok'
}

export { set, SetOptions }
