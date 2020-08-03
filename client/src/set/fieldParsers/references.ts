import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
import { _set } from '../index'
import { SetOptions } from '../types'
import { Schema, FieldSchemaArrayLike } from '../../schema'
import parseSetObject from '../validate'
import { verifiers } from './simple'
import { setRecordDef } from '../modifyDataRecords'

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

const parseObjectArray = async (
  client: SelvaClient,
  payload: any,
  schema: Schema,
  $lang?: string
) => {
  if (Array.isArray(payload) && typeof payload[0] === 'object') {
    return Promise.all(
      payload.map(ref => parseSetObject(client, ref, schema, $lang))
    )
  }
}

const toCArr = async (
  client: SelvaClient,
  schema: Schema,
  result: any,
  setObj:
    | ({ [index: string]: string } | string)[]
    | undefined
    | null,
  noRoot: boolean
) => {
    setObj
  if (!setObj) {
    return null
  }

  const ids = setObj.length && typeof setObj[0] === 'object'
    ? await Promise.all(setObj.map((node: any) => {
        node[0] = `N${node[0].substring(1)}`

        return _set(client, node, schema.sha, result.$db)
    }))
    : setObj

  return ids
    .filter((s: string | null) => !!s)
    .map((s: string) => s.padEnd(10, '\0'))
    .join('')
}

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: string[],
  _fields: FieldSchemaArrayLike,
  _type: string,
  $lang?: string
): Promise<void> => {
  let noRoot = false
  const r: SetOptions = {}
  const isEmpty = (v: any) => !v || !v.length

  if (
    typeof payload === 'object' &&
    !Array.isArray(payload) &&
    payload !== null
  ) {
    let hasKeys = false
    result[field] = {}
    for (let k in payload) {
      if (k === '$add') {
        const parsed = await parseObjectArray(client, payload[k], schema, $lang)
        if (parsed) {
          r.$add = parsed
          hasKeys = true
        } else if (
          typeof payload[k] === 'object' &&
          !Array.isArray(payload[k])
        ) {
          r.$add = [
            await parseSetObject(client, payload[k], schema, $lang)
          ]
          hasKeys = true
        } else {
          if (payload[k].length) {
            r.$add = verifySimple(payload[k])
            hasKeys = true
          }
        }
      } else if (k === '$delete') {
        if (payload.$delete === true) {
          r.delete_all = 1
        } else {
          r.$delete = verifySimple(payload[k])
        }

        hasKeys = true
      } else if (k === '$value') {
        r.$value = verifySimple(payload[k])
        hasKeys = true
      } else if (k === '$hierarchy') {
        if (payload[k] !== false && payload[k] !== true) {
          throw new Error(
            `Wrong payload for references ${JSON.stringify(payload)}`
          )
        }
        r.$hierarchy = payload[k]
        hasKeys = true
      } else if (k === '$noRoot') {
        if (typeof payload[k] !== 'boolean') {
          throw new Error(`Wrong payload type for $noRoot in references ${k}`)
        }

        r.$noRoot = payload[k]
        hasKeys = true
        if (field === 'parents') {
          noRoot = payload[k]
        }
      } else if (k === '$_itemCount') {
        // ignore this internal field if setting with a split payload
      } else {
        throw new Error(`Wrong key for references ${k}`)
      }
    }

    if (hasKeys) {
      result.push(
        '5',
        field,
        createRecord(setRecordDef, {
          is_reference: 1,
          delete_all: r.delete_all || (isEmpty(r.$add) && isEmpty(r.$delete) && isEmpty(r.$value)),
          $add: await toCArr(client, schema, result, r.$add, noRoot),
          $delete: await toCArr(client, schema, result, r.$delete, noRoot),
          $value: await toCArr(client, schema, result, r.$value, noRoot)
        }).toString()
      )
    }
  } else {
    const r = (await parseObjectArray(client, payload, schema, $lang)) || verifySimple(payload)
    const $value = await toCArr(client, schema, result, r, noRoot)

    result.push(
      '5',
      field,
      createRecord(setRecordDef, {
        is_reference: 1,
        delete_all: isEmpty($value),
        $add: '',
        $delete: '',
        $value,
      }).toString()
    )
  }
}
