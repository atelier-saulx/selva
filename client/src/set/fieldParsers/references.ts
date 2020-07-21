import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
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

const parseObjectArray = async (client: SelvaClient, payload: any, schema: Schema, $lang?: string) => {
  if (Array.isArray(payload) && typeof payload[0] === 'object') {
    return Promise.all(payload.map(ref => parseSetObject(client, ref, schema, $lang)))
  }
}

const toCArr = async (
  client: SelvaClient,
  setObj:
    { [index: string]: string } |
    { [index: string]: string }[] |
    string[] |
    undefined |
    null,
  noRoot: boolean) => {
  let o: any[];

  if (!setObj) {
    return ''
  } else if (typeof setObj === 'string') {
    o = [ setObj ]
  } else if (Array.isArray(setObj)) {
    o = setObj
  } else if (typeof setObj === 'object') {
    o = [ setObj ]
  } else {
    return ''
  }

  const ids = (await Promise.all(o.map(async e => {
    if (typeof e === 'string') {
      return e
    }
    if (e.$id) {
      return e.$id
    }
    if (e.$args) {
      if (e.$args[1] !== '$alias') {
        throw new Error('Invalid format for a reference')
      }
      const alias = e.$args[2]
      const type = e.type

      let { id } = await client.get({ $alias: alias.substring(0, alias.length - 1), id: true })
      if (!id && type) {
        id = await client.set({
          type,
          aliases: [ alias ],
          parents: {
            $noRoot: !!noRoot
          }
        });
      }

      return id;
    }
    return null;
  }))).filter((s: string | null) => s && !s.startsWith('$'))

  return ids.filter((s: string | null) => s && !s.startsWith('$')).map((s: string) => s.padEnd(10, '\0')).join('')
}

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  _fields: FieldSchemaArrayLike,
  _type: string,
  $lang?: string
): Promise<void> => {
  const isReference = ['children', 'parents'].includes(field)
  let noRoot = false;

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
          result[field].$add = parsed
          hasKeys = true
        } else if (
          typeof payload[k] === 'object' &&
          !Array.isArray(payload[k])
        ) {
          result[field].$add = [await parseSetObject(client, payload[k], schema, $lang)]
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
        result[field].$value = verifySimple(payload[k])
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
        if (field === 'parents') {
          noRoot = payload[k];
        }
      } else if (k === '$_itemCount') {
        // ignore this internal field if setting with a split payload
      } else {
        throw new Error(`Wrong key for references ${k}`)
      }
    }

    if (hasKeys) {
      result.$args.push('5', field, createRecord(setRecordDef, {
          is_reference: isReference,
          $add: await toCArr(client, result[field].$add, noRoot),
          $delete: await toCArr(client, result[field].$delete, noRoot),
          $value: await toCArr(client, result[field].$value, noRoot),
      }).toString())
    } else {
      delete result[field]
    }
  } else {
    result[field] =
      await parseObjectArray(client, payload, schema, $lang) || verifySimple(payload)

    if (Array.isArray(result[field])) {
      const referenceCount = result[field].reduce((acc: number, x) => {
        return acc + (x.$_itemCount || 1)
      }, 0)

      result.$_itemCount = (result.$_itemCount || 1) + referenceCount
      result[field].$_itemCount = referenceCount
    }

    result.$args.push('5', field, createRecord(setRecordDef, {
      is_reference: isReference,
      $add: '',
      $delete: '',
      $value: await toCArr(client, result[field], noRoot),
    }).toString())
  }
}
