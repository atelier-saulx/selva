import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, FieldSchemaOther } from '../../schema'
import digest from '../../digest'
import {
  incrementDef,
  incrementDoubleDef,
  longLongDef,
  doubleDef,
} from '../modifyDataRecords'
import * as verifiers from '@saulx/validators'

const converters = {
  digest,
  timestamp: (payload: 'now' | number): Buffer =>
    createRecord(longLongDef, {
      d: BigInt(payload === 'now' ? Date.now() : payload),
    }),
  boolean: (v: boolean): Buffer => createRecord(longLongDef, { d: BigInt(+v) }),
  int: (payload: number): Buffer =>
    createRecord(longLongDef, { d: BigInt(payload) }),
  float: (payload: number): Buffer => createRecord(doubleDef, { d: payload }),
  number: (payload: number): Buffer => createRecord(doubleDef, { d: payload }),
}

const VALUE_TYPE_TO_DEFAULT_VALUE_TYPE = {
  3: '8',
  A: '9',
  0: '2',
}

const parsers = {}

for (const key in verifiers) {
  const verify = verifiers[key]

  const valueType: string = ['boolean', 'int', 'timestamp'].includes(key)
    ? '3'
    : ['float', 'number'].includes(key)
    ? 'A'
    : '0'
  const isNumber = key === 'float' || key === 'number' || key === 'int'
  const noOptions = key === 'type' || key === 'id' || key === 'digest'
  const converter = converters[key]

  parsers[key] = (
    client: SelvaClient,
    schema: Schema,
    field: string,
    payload: SetOptions,
    result: (string | Buffer)[],
    _fields: FieldSchemaOther,
    type: string,
    lang: string
  ) => {
    const keyname: string = field
    let value: string | null = null

    if (!noOptions && typeof payload === 'object') {
      let hasKeys = false
      for (const k in payload) {
        value = payload[k]
        hasKeys = true
        if (isNumber && k === '$increment') {
          if (payload[k].$ref) {
            if (typeof payload[k].$ref !== 'string') {
              throw new Error(`Non-string $ref provided for ${key}.${k}`)
            }

            value = `___selva_$ref:${payload[k].$ref}`
          } else {
            if (
              client.validator
                ? client.validator(
                    schema,
                    type,
                    field.split('.'),
                    payload[k],
                    lang
                  ) && verify(payload[k])
                : verify(payload[k])
            ) {
              throw new Error(`Incorrect payload for ${key}.${k} ${payload}`)
            } else if (
              converter &&
              !['int', 'float', 'number'].includes(key) // createRecord will take care of numbers
            ) {
              value = converter(payload[k])
            }

            if (key === 'int') {
              result.push(
                '4',
                field,
                createRecord(incrementDef, {
                  $default: isNaN(payload.$default)
                    ? 0
                    : Number(payload.$default),
                  $increment: isNaN(payload.$increment)
                    ? 0
                    : Number(payload.$increment),
                })
              )
            } else {
              result.push(
                'B',
                field,
                createRecord(incrementDoubleDef, {
                  $default: isNaN(payload.$default)
                    ? 0
                    : Number(payload.$default),
                  $increment: isNaN(payload.$increment)
                    ? 0
                    : Number(payload.$increment),
                })
              )
            }
            return 1
          }
        } else if (k === '$ref') {
          value = `___selva_$ref:${payload[k]}`
          return 0
        } else if (k === '$delete') {
          result.push('7', field, '')
          return 0
        } else if (k === '$default') {
          // let this be handled below
        } else {
          throw new Error(`Incorrect payload for ${key} incorrect field ${k}`)
        }
      }

      if (payload.$default) {
        if (
          client.validator
            ? client.validator(
                schema,
                type,
                field.split('.'),
                payload.$default,
                lang
              ) && verify(payload.$default)
            : verify(payload.$default)
        ) {
          if (converter) {
            value = converter(payload.$default)
          } else {
            value = String(payload.$default)
          }
        } else {
          throw new Error(
            `Incorrect payload for ${field} of type ${key}: ${JSON.stringify(
              payload
            )}`
          )
        }

        const defaultValueType = VALUE_TYPE_TO_DEFAULT_VALUE_TYPE[valueType]
        if (defaultValueType) {
          result.push(defaultValueType, keyname, value)
        }

        return 1
      }

      if (!hasKeys) {
        throw new Error(`Incorrect payload empty object for field ${field}`)
      }
    } else if (
      client.validator
        ? client.validator(schema, type, field.split('.'), payload, lang) &&
          verify(payload)
        : verify(payload)
    ) {
      if (converter) {
        value = converter(payload)
      } else {
        value = String(payload)
      }
    } else {
      throw new Error(
        `Incorrect payload for ${field} of type ${key}: ${JSON.stringify(
          payload
        )}`
      )
    }

    if (value !== null && value !== undefined) {
      result.push(valueType, keyname, value)
      return 1
    }

    return 0
  }
}

export default parsers
