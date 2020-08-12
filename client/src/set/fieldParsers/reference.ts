import { SelvaClient } from '../..'
import parseSetObject from '../validate'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike } from '../../schema'
import { verifiers } from './simple'

const id = verifiers.id

const verifySimple = (payload: any) => {
  if (id(payload)) {
    return payload
  } else {
    throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
  }
}

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  _fields: FieldSchemaArrayLike,
  _type: string,
  $lang?: string
): Promise<void> => {
  if (typeof payload === 'object') {
    if (Array.isArray(payload)) {
      throw new Error(
        `Wrong payload for reference ${JSON.stringify(
          payload
        )}, should be an object or id string`
      )
    }

    if (payload.$delete === true) {
      result.push('7', field, '')
      return
    }

    if ($lang) {
      payload.$language = $lang
    }

    if ((<any>result).$db) {
      payload.$db = (<any>result).$db
    }

    if (!payload.$id && payload.$alias) {
      const id = await client.set(payload)
      result.push('0', field, id)
    } else if (!payload.$id && payload.type) {
      const id = await client.id({
        db: (<any>result).$db || 'default',
        type: payload.type
      })

      const obj = { ...payload, $id: id }
      ;(<any>result).$extraQueries.push(client.set(obj))
      result.push('0', field, id)
    } else if (payload.$id) {
      ;(<any>result).$extraQueries.push(client.set(payload))
      result.push('0', field, payload.$id)
    } else {
      throw new Error(
        `Type or $id or $alias required for nested single reference in ${field}, got: ${JSON.stringify(
          payload,
          null,
          2
        )}`
      )
    }
  } else {
    if (typeof payload !== 'string') {
      throw new Error(
        `Wrong payload for reference ${JSON.stringify(
          payload
        )}, should be an object or id string`
      )
    }

    result.push('0', field, verifySimple(payload))
  }
}
