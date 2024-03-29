import { SetOptions, SetMetaResponse } from './types'
import { Schema, SelvaClient } from '..'
import parseSetObject from './validate'

export async function _set(
  client: SelvaClient,
  payload: (string | Buffer)[],
  schemaSha: string,
  db?: string,
  meta: boolean = false
): Promise<string | SetMetaResponse> {
  const asAny = <any>payload

  if (!asAny.$id) {
    asAny.$id = await client.id({ db, type: asAny.$type })
  }

  // console.log('ID', asAny.$id, 'PAYLOAD', JSON.stringify(payload))

  // console.log('amount of extra queries', asAny.$extraQueries.length)

  // TODO: remove this, we already batch in drainQueue
  // if (asAny.$extraQueries.length > 5000) {
  //   console.log('batching')
  //   while (asAny.$extraQueries.length >= 5000) {
  //     console.log('batching, still left:', asAny.$extraQueries.length)
  //     const batch = asAny.$extraQueries.splice(0, 5000)
  //     await Promise.all(batch)
  //   }
  // }

  // console.log('main set, still left:', asAny.$extraQueries.length)
  // TODO: end remove
  //
  try {
    const all = await Promise.all([
      client.redis.selva_modify(
        { name: db || 'default', type: 'origin' },
        asAny.$id,
        ...payload
      ),
      ...asAny.$extraQueries,
    ])

    if (meta) {
      let anyUpdated = false
      const changed: Set<string> = new Set()
      const unchanged: Set<string> = new Set()
      const errored: Record<string, string> = {}
      for (let i = 2, j = 1; i < payload.length; i += 3, j++) {
        if (all[0][j] === 'UPDATED') {
          anyUpdated = true
          changed.add(<string>payload[i])
        } else if (all[0][j] === 'OK') {
          unchanged.add(<string>payload[i])
        } else {
          errored[<string>payload[i]] = 'ERROR' // TODO: real error
        }
      }

      return {
        id: all[0][0],
        updated: anyUpdated,
        fields: {
          changed,
          unchanged,
          errored,
        },
      }
    }

    return !all[0] ? undefined : all[0][0]
  } catch (err) {
    console.error('Modify failed:', err)
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

async function set(
  client: SelvaClient,
  payload: SetOptions,
  schema?: Schema
): Promise<string> {
  if (!schema) {
    schema = client.schemas[payload.$db || 'default']
  }

  // need to add queue and process.next here to merge modify
  if (!payload.type && !payload.$id && payload.$alias) {
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

  return <Promise<string>>_set(client, parsed, schema.sha, payload.$db, false)
}

async function setWithMeta(
  client: SelvaClient,
  payload: SetOptions
): Promise<SetMetaResponse> {
  const schema = client.schemas[payload.$db || 'default']

  // need to add queue and process.next here to merge modify
  if (!payload.type && !payload.$id && payload.$alias) {
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

  return <Promise<SetMetaResponse>>(
    _set(client, parsed, schema.sha, payload.$db, true)
  )
}

export { set, setWithMeta, SetOptions }
