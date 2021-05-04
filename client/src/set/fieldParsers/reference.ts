import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
import parseSetObject from '../validate'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike } from '../../schema'
import { verifiers } from './simple'
import { OPT_SET_TYPE, setRecordDefCstring } from '../modifyDataRecords'

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
): Promise<number> => {
  let id
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
      return 0
    }

    if ($lang) {
      payload.$language = $lang
    }

    if ((<any>result).$db) {
      payload.$db = (<any>result).$db
    }

    if (!payload.$id && payload.$alias) {
      id = await client.set(payload)
    } else if (!payload.$id && payload.type) {
      id = await client.id({
        db: (<any>result).$db || 'default',
        type: payload.type,
      })

      const obj = { ...payload, $id: id }
      ;(<any>result).$extraQueries.push(client.set(obj))
    } else if (payload.$id) {
      ;(<any>result).$extraQueries.push(client.set(payload))
      id = payload.$id
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

    id = verifySimple(payload)
  }

  result.push(
    '5',
    field,
    createRecord(setRecordDefCstring, {
      op_set_type: OPT_SET_TYPE.reference,
      constraint_id: 1,
      delete_all: false,
      $add: '',
      $delete: '',
      $value: id.padEnd(10, '\0'),
    })
  )

  return 1
}
