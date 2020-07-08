import { compile, createRecord } from 'data-record'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike } from '../../schema'
import parseSetObject from '../validate'
import parsers from './simple'

const setRecordDef = compile([
  { name: 'is_reference', type: 'int8' },
  { name: '$add', type: 'cstring_p' },
  { name: '$delete', type: 'cstring_p' },
  { name: '$value', type: 'cstring_p' },
], { align: true })

const verifySimple = (payload, verify) => {
  if (Array.isArray(payload)) {
    return payload.map(v => verify(v))
  } else {
    return [verify(payload)]
  }
}

const parseObjectArray = (payload: any, schema: Schema) => {
  if (Array.isArray(payload) && typeof payload[0] === 'object') {
    return payload.map(ref => parseSetObject(ref, schema))
  }
}

const toCArr = (arr: string[] | undefined | null, isId?: boolean) =>
    arr
      ? arr.map(s => isId ? s.padEnd(10, '\0') : `${s}\0`).join('')
      : ''

// function isArrayLike(x: any): x is FieldSchemaArrayLike {
//   return x && !!x.items
// }

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaArrayLike,
  type: string
): void => {
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

  const verify = v => {
    const r: { value: any } = { value: undefined }
    parser(schema, 'value', v, r, fields, type)
    return r.value
  }

  if (typeof payload === 'object' && !Array.isArray(payload)) {
    let r: SetOptions = {};

    for (let k in payload) {
      if (k === '$add') {
        const parsed = parseObjectArray(payload[k], schema)
        if (parsed) {
          r.$add = parsed
        } else if (
          typeof payload[k] === 'object' &&
          !Array.isArray(payload[k])
        ) {
          r.$add = [parseSetObject(payload[k], schema)]
        } else {
          r.$add = verifySimple(payload[k], verify)
        }
      } else if (k === '$delete') {
        if (payload.$delete === true) {
          // unsets are allowed
          r.$delete = true // FIXME
        } else {
          r.$delete = verifySimple(payload[k], verify)
        }
      } else {
        throw new Error(`Wrong key for set ${k}`)
      }
    }

    result.$args.push('5', field, createRecord(setRecordDef, {
        is_reference: 0,
        $add: toCArr(r.$add, false),
        $delete: toCArr(r.$delete, false),
        $value: '',
    }).toString());
  } else {
    result.$args.push('5', field, createRecord(setRecordDef, {
        is_reference: 0,
        $add: '',
        $delete: '',
        $value: toCArr(parseObjectArray(payload, schema) || verifySimple(payload, verify), false)
    }).toString());
  }
}
