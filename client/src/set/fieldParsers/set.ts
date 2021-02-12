import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
import {
  OPT_SET_TYPE,
  setRecordDefCstring,
  setRecordDefDouble,
  setRecordDefInt64,
} from '../modifyDataRecords'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike, FieldType } from '../../schema'
import parseSetObject from '../validate'
import parsers from './simple'

const doubleTypes: FieldType[] = ['number', 'float']
const intTypes: FieldType[] = ['int', 'timestamp']

const verifySimple = async (
  payload: SetOptions,
  verify: (p: SetOptions) => Promise<any>
) => {
  if (Array.isArray(payload)) {
    return Promise.all(payload.map((v) => verify(v)))
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
    return Promise.all(
      payload.map((ref) => parseSetObject(client, ref, schema))
    )
  }
}

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchemaArrayLike,
  type: string
): Promise<void> => {
  const typeSchema = type === 'root' ? schema.rootType : schema.types[type]
  if (!typeSchema) {
    throw new Error('Cannot find type schema ' + type)
  }

  // 'string' is just a guess here if we don't know the type but it's probably the right one
  // @ts-ignore
  const elementType = typeSchema.fields[field]?.items?.type || 'string'
  // @ts-ignore
  const [setRecordDef, opSetType, toCArr] = doubleTypes.includes(elementType)
    ? [setRecordDefDouble, OPT_SET_TYPE.double, (arr: any) => arr]
    : // @ts-ignore
    intTypes.includes(elementType)
    ? [
        setRecordDefInt64,
        OPT_SET_TYPE.long_long,
        (arr: number[] | undefined | null) => (arr ? arr.map(BigInt) : arr),
      ]
    : [
        setRecordDefCstring,
        OPT_SET_TYPE.char,
        (arr: string[] | undefined | null) =>
          arr ? arr.map((s) => `${s}\0`).join('') : '',
      ]

  if (!fields || !fields.items) {
    throw new Error(`Cannot find field ${field} on ${type}`)
  }
  const fieldType = fields.items.type
  const parser = parsers[fieldType]
  if (!parser) {
    throw new Error(`Cannot find parser for ${fieldType}`)
  }

  const verify = async (v: SetOptions) => {
    const r: string[] = []
    await parser(client, schema, 'value', v, r, fields, type)
    return r[2]
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
          // TODO: do these modify commands recursively and then populate the ids here
          // r.$add = [await parseSetObject(client, payload[k], schema)]
        } else {
          r.$add = await verifySimple(payload[k], verify)
        }
      } else if (k === '$delete') {
        if (payload.$delete === true) {
          // unsets are allowed
          r.delete_all = 1
        } else {
          r.$delete = await verifySimple(payload[k], verify)
        }
      } else {
        throw new Error(`Wrong key for set ${k}`)
      }
    }

    result.push(
      '5',
      field,
      createRecord(setRecordDef, {
        op_set_type: opSetType,
        delete_all: r.delete_all,
        $add: toCArr(r.$add),
        $delete: toCArr(r.$delete),
        $value: null,
      })
    )
  } else {
    result.push(
      '5',
      field,
      createRecord(setRecordDef, {
        op_set_type: opSetType,
        $add: null,
        $delete: null,
        $value: toCArr(
          opSetType === OPT_SET_TYPE.char
            ? (await parseObjectArray(client, payload, schema)) ||
                (await verifySimple(payload, verify))
            : payload
        ),
      })
    )
  }
}
