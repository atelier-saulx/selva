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

const addParent = (obj: Record<string, any>, id: string) => {
  if (!obj.parents) {
    obj.parents = [id]
  } else {
    if (typeof obj.parents === 'string') {
      obj.parents = [obj.parents, id]
    } else if (Array.isArray(obj.parents)) {
      obj.parents.push(id)
    } else {
      if (obj.parents.$add) {
        if (typeof obj.parents.$add === 'string') {
          obj.parents.$add = [obj.parents.$add, id]
        } else {
          obj.parents.$add.push(id)
        }
      } else if (obj.parents.$delete) {
        obj.parents.$add.push(id)
      } else if (obj.parents.$value) {
        if (typeof obj.parents.$value === 'string') {
          obj.parents.$value = [obj.parents.$value, id]
        } else {
          obj.parents.$value.push(id)
        }
      }
    }
  }
}

const toCArr = async (
  client: SelvaClient,
  schema: Schema,
  result: any,
  setObj: ({ [index: string]: any } | string)[] | string | undefined | null,
  noRoot: boolean,
  lang?: string
) => {
  if (!setObj) {
    return ''
  }

  if (typeof setObj === 'string') {
    return setObj.padEnd(10, '\0')
  }

  const ids: string[] = []
  for (const obj of setObj) {
    if (typeof obj === 'string') {
      ids.push(obj)
    } else if (obj.$id) {
      ids.push(obj.$id)

      addParent(obj, result.$id)

      if (lang) {
        obj.$language = lang
      }

      if (result.$db) {
        obj.$db = result.$db
      }

      ;(<any>result).$extraQueries.push(client.set(obj))
    } else if (obj.$alias) {
      if (lang) {
        obj.$language = lang
      }

      if (result.$db) {
        obj.$db = result.$db
      }

      addParent(obj, result.$id)

      const id = await client.set(obj)
      ids.push(id)
    } else if (obj.type) {
      const id = await client.id({ type: obj.type })
      obj.$id = id
      ids.push(id)

      if (result.$db) {
        obj.$db = result.$db
      }

      addParent(obj, result.$id)

      // non-blocking set
      if (lang) {
        obj.$language = lang
      }

      ;(<any>result).$extraQueries.push(client.set(obj))
    } else {
      throw new Error('Missing $id/$alias/type in nested payload')
    }
  }

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
  result: (string | Buffer)[],
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
        if (typeof payload[k] === 'object' && !Array.isArray(payload[k])) {
          r.$add = [payload[k]]
          hasKeys = true
        } else {
          r.$add = payload[k]
          hasKeys = true
        }
      } else if (k === '$delete') {
        if (payload.$delete === true) {
          r.delete_all = 1
        } else {
          r.$delete = verifySimple(payload[k])
        }

        hasKeys = true
      } else if (k === '$value') {
        if (typeof payload[k] === 'object' && !Array.isArray(payload[k])) {
          r.$value = [payload[k]]
          hasKeys = true
        } else {
          r.$value = payload[k]
          hasKeys = true
        }
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
          delete_all:
            r.delete_all || (!r.$add && !r.$delete && isEmpty(r.$value)),
          $add: await toCArr(client, schema, result, r.$add, noRoot, $lang),
          $delete: await toCArr(
            client,
            schema,
            result,
            r.$delete,
            noRoot,
            $lang
          ),
          $value: await toCArr(client, schema, result, r.$value, noRoot, $lang)
        })
      )
    }
  } else {
    let r: any
    if (typeof payload === 'object' && !Array.isArray(payload)) {
      r = [payload]
    } else {
      r = payload
    }
    const $value = await toCArr(client, schema, result, r, noRoot, $lang)

    result.push(
      '5',
      field,
      createRecord(setRecordDef, {
        is_reference: 1,
        delete_all: isEmpty($value),
        $add: '',
        $delete: '',
        $value
      })
    )
  }
}
