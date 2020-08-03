import { SetOptions } from './types'
import { SelvaClient } from '..'
import { v4 as uuid } from 'uuid'
import { SCRIPT } from '../constants'
import parseSetObject from './validate'

export async function _set(
  client: SelvaClient,
  payload: string[],
  schemaSha: string,
  db?: string
): Promise<string> {
  console.log('PAYLOAD', JSON.stringify(payload))
  const asAny = <any>payload
  if (!asAny.$id) {
    asAny.$id = await client.id({ db, type: asAny.$type })
  }

  try {
    return await client.redis.selva_modify(
      { name: db || 'default' },
      asAny.$id,
      ...payload
    )
  } catch (err) {
    console.error(err)
    throw err
  }
}

// make this the internal set
// then add a queue with resolve / reject -- will combine it there are multipiple (array)

// handle

// alias
// validation
// children inserting them
// combine $add and $delete
// $ref
// references type (+alias)
// compound increment / decrement
// value ? strip it out
//

async function set(client: SelvaClient, payload: SetOptions): Promise<string> {
  const schema = client.schemas[payload.$db || 'default']

  // need to add queue and process.next here to merge modify
  if (!payload.$type && !payload.$id && payload.$alias) {
    let aliases = payload.$alias
    if (!Array.isArray(payload.$alias)) {
      aliases = [aliases]
    }

    for (const alias of aliases) {
      const id = await client.redis.hget(`___selva_aliases`, alias)
      if (id) {
        payload.$id = id
        break
      }
    }

    if (!payload.$id) {
      throw new Error(
        `.set() without the type property requires an existing record or $id to be set with the wanted type prefix. No existing id found for alias ${JSON.stringify(
          payload.$alias
        )}`
      )
    }
  }

  // nested aliases need to be supported

  // refactor this whole thign

  const parsed = await parseSetObject(client, payload, schema)
  ;(<any>parsed).$type = payload.type

  return _set(client, parsed, schema.sha, payload.$db)
}

export { set, SetOptions }
