import { createRecord } from 'data-record'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike } from '../../schema'
import parseSetObject from '../validate'
import { verifiers } from './simple'
import { setRecordDef } from './set'

const id = verifiers.id

const verifySimple = payload => {
  if (Array.isArray(payload)) {
    if (payload.find(v => !id(v))) {
      throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
    }
    return payload
  } else if (id(payload)) {
    return [payload]
  } else {
    throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
  }
}

const parseObjectArray = (payload: any, schema: Schema, $lang?: string) => {
  if (Array.isArray(payload) && typeof payload[0] === 'object') {
    return payload.map(ref => parseSetObject(ref, schema, $lang))
  }
}

const toCArr = (setObj: { [index: string]: string } | undefined | null) =>
  setObj ? Object.keys(setObj).filter(k => !k.startsWith('$')).map(k => setObj[k].padEnd(10, '\0')).join('') : ''

export default (
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  _fields: FieldSchemaArrayLike,
  _type: string,
  $lang?: string
): void => {
  const isReference = ['children', 'parents'].includes(field)

  if (
    typeof payload === 'object' &&
    !Array.isArray(payload) &&
    payload !== null
  ) {
    let hasKeys = false
    result[field] = {}
    for (let k in payload) {
      if (k === '$add') {
        const parsed = parseObjectArray(payload[k], schema, $lang)
        if (parsed) {
          result[field].$add = parsed
          hasKeys = true
        } else if (
          typeof payload[k] === 'object' &&
          !Array.isArray(payload[k])
        ) {
          result[field].$add = [parseSetObject(payload[k], schema, $lang)]
          hasKeys = true
        } else {
          if (payload[k].length) {
            result[field].$add = verifySimple(payload[k])
            hasKeys = true
          }
        }
      } else if (k === '$delete') {
        if (payload.$delete === true) {
          result[field].$delete = true
        } else {
          result[field].$delete = verifySimple(payload[k])
        }

        hasKeys = true
      } else if (k === '$value') {
        result[field].$delete = verifySimple(payload[k])
        hasKeys = true
      } else if (k === '$hierarchy') {
        if (payload[k] !== false && payload[k] !== true) {
          throw new Error(
            `Wrong payload for references ${JSON.stringify(payload)}`
          )
        }
        result[field].$hierarchy = payload[k]
        hasKeys = true
      } else if (k === '$noRoot') {
        if (typeof payload[k] !== 'boolean') {
          throw new Error(`Wrong payload type for $noRoot in references ${k}`)
        }

        result[field].$noRoot = payload[k]
        hasKeys = true
      } else if (k === '$_itemCount') {
        // ignore this internal field if setting with a split payload
      } else {
        throw new Error(`Wrong key for references ${k}`)
      }
    }

    if (hasKeys) {
      result.$args.push('5', field, createRecord(setRecordDef, {
          is_reference: isReference,
          $add: toCArr(result[field].$add),
          $delete: toCArr(result[field].$delete),
          $value: '',
      }).toString())
    } else {
      delete result[field]
    }
  } else {
    result[field] =
      parseObjectArray(payload, schema, $lang) || verifySimple(payload)

    if (Array.isArray(result[field])) {
      const referenceCount = result[field].reduce((acc, x) => {
        return acc + (x.$_itemCount || 1)
      }, 0)

      result.$_itemCount = (result.$_itemCount || 1) + referenceCount
      result[field].$_itemCount = referenceCount
    }

    result.$args.push('5', field, createRecord(setRecordDef, {
      is_reference: isReference,
      $add: '',
      $delete: '',
      $value: toCArr(result[field]),
    }).toString())
  }
}
