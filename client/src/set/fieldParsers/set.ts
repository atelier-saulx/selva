import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
import { setRecordDef } from '../modifyDataRecords'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike } from '../../schema'
import parseSetObject from '../validate'
import parsers from './simple'

const verifySimple = async (
  payload: SetOptions,
  verify: (p: SetOptions) => Promise<any>
) => {
  if (Array.isArray(payload)) {
    return Promise.all(payload.map(v => verify(v)))
  } else {
    return [await verify(payload)]
  }
}

const parseObjectArray = async (
  client: SelvaClient,
  payload: any,
  schema: Schema
) => {
  if (Array.isArray(payload) && typeof payload[0] === 'object') {
    return Promise.all(payload.map(ref => parseSetObject(client, ref, schema)))
  }
}

const toCArr = (arr: string[] | undefined | null) =>
  arr ? arr.map(s => `${s}\0`).join('') : ''

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string
): Promise<void> => {
  if (!result.$args) result.$args = []

  const typeSchema = type === 'root' ? schema.rootType : schema.types[type]
  if (!typeSchema) {
    throw new Error('Cannot find type schema ' + type)
  }

  if (!fields || !fields.items) {
    throw new Error(`Cannot find field ${field} on ${type}`)
  }
  const fieldType = fields.items.type
  const parser = parsers[fieldType]
  if (!parser) {
    throw new Error(`Cannot find parser for ${fieldType}`)
  }

  const verify = async (v: SetOptions) => {
    const r: { value: any } = { value: undefined }
    await parser(client, schema, 'value', v, r, fields, type)
    return r.value
  }

  if (typeof payload === 'object' && !Array.isArray(payload)) {
    let r: SetOptions = {}

    for (let k in payload) {
      if (k === '$add') {
        const parsed = await parseObjectArray(client, payload[k], schema)
        if (parsed) {
          r.$add = parsed
        } else if (
          typeof payload[k] === 'object' &&
          !Array.isArray(payload[k])
        ) {
          r.$add = [await parseSetObject(client, payload[k], schema)]
        } else {
          r.$add = await verifySimple(payload[k], verify)
        }
      } else if (k === '$delete') {
        if (payload.$delete === true) {
          // unsets are allowed
          r.$delete = true // FIXME
        } else {
          r.$delete = await verifySimple(payload[k], verify)
        }
      } else {
        throw new Error(`Wrong key for set ${k}`)
      }
    }

    result.$args.push(
      '5',
      field,
      createRecord(setRecordDef, {
        is_reference: 0,
        $add: toCArr(r.$add),
        $delete: toCArr(r.$delete),
        $value: ''
      }).toString()
    )
  } else {
    result.$args.push(
      '5',
      field,
      createRecord(setRecordDef, {
        is_reference: 0,
        $add: '',
        $delete: '',
        $value: toCArr(
          (await parseObjectArray(client, payload, schema)) ||
            (await verifySimple(payload, verify))
        )
      }).toString()
    )
  }
}
