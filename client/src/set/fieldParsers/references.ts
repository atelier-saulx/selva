import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { getNestedSchema } from '../../get/utils'
import { Schema, FieldSchemaReferences } from '../../schema'
import * as verifiers from '@saulx/validators'
import { OPT_SET_TYPE, setRecordDefCstring } from '../modifyDataRecords'
import { padId } from '../../util'

const id = verifiers.id

const verifySimple = (payload) => {
  if (Array.isArray(payload)) {
    if (payload.find((v) => !id(v))) {
      throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
    }
    return payload
  } else if (id(payload)) {
    return [payload]
  } else {
    throw new Error(`Wrong payload for references ${JSON.stringify(payload)}`)
  }
}

const addParent = (
  field: string,
  anyResult: any,
  obj: Record<string, any>,
  id: string
) => {
  if (field === 'parents') {
    const noRoot: boolean = anyResult[0][0] === 'N'
    if (!obj.parents) {
      obj.parents = {
        $add: [],
        $noRoot: true,
      }
    } else if (noRoot) {
      if (typeof obj.parents === 'string') {
        obj.parents = { $value: [obj.parents], $noRoot: true }
      } else if (Array.isArray(obj.parents)) {
        obj.parents = { $value: obj.parents, $noRoot: true }
      } else {
        obj.parents.$noRoot = true
      }
    }
    return
  }

  if (field !== 'children') {
    return
  }

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
  field: string,
  fields: FieldSchemaReferences,
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
    return padId(setObj)
  }

  const ids: string[] = []
  for (const obj of setObj) {
    if (typeof obj === 'string') {
      ids.push(obj)
    } else if (obj.$id) {
      ids.push(obj.$id)

      addParent(field, result, obj, result.$id)

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

      addParent(field, result, obj, result.$id)

      const id = await client.set(obj)
      ids.push(id)
    } else if (obj.type) {
      const id = await client.id({
        type: obj.type,
        db: obj.$db || result.$db || 'default',
      })
      obj.$id = id
      ids.push(id)

      if (result.$db) {
        obj.$db = result.$db
      }

      addParent(field, result, obj, result.$id)

      // non-blocking set
      if (lang) {
        obj.$language = lang
      }

      ;(<any>result).$extraQueries.push(client.set(obj))
    } else {
      throw new Error('Missing $id/$alias/type in nested payload')
    }
  }

  if (fields.bidirectional) {
    // ADD VERIFY

    const fromField = fields.bidirectional.fromField

    for (const id of ids) {
      const targetField = getNestedSchema(schema, id, fromField)
      if (
        !targetField ||
        (targetField.type !== 'reference' &&
          targetField.type !== 'references') ||
        !targetField.bidirectional
      ) {
        throw new Error(
          `Wrong payload for reference ${JSON.stringify(
            setObj
          )}, bidirectional reference requires a bidirectional target field ${fromField} for id ${id}`
        )
      }
    }
  }

  return ids
    .filter((s: string | null) => !!s)
    .map((s: string) => padId(s))
    .join('')
}

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchemaReferences,
  type: string,
  $lang?: string
): Promise<number> => {
  let noRoot = field === 'parents'
  const r: SetOptions = {}
  const isEmpty = (v: any) => !v || !v.length

  // TODO: fix noRoot

  if (
    typeof payload === 'object' &&
    !Array.isArray(payload) &&
    payload !== null
  ) {
    let hasKeys = false
    result[field] = {}
    for (const k in payload) {
      if (k === '$add') {
        if (typeof payload[k] === 'object' && !Array.isArray(payload[k])) {
          r.$add = [payload[k]]
          hasKeys = true
        } else {
          r.$add = payload[k]
          hasKeys = true
        }

        if (
          client.validator &&
          !client.validator(schema, type, field.split('.'), r.$add, $lang)
        ) {
          throw new Error(
            'Incorrect payload for "references.$add" from custom validator'
          )
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

        if (
          client.validator &&
          !client.validator(schema, type, field.split('.'), r.$value, $lang)
        ) {
          throw new Error(
            'Incorrect payload for "references.$value" from custom validator'
          )
        }
      } else if (k === '$hierarchy') {
        if (payload[k] !== false && payload[k] !== true) {
          throw new Error(
            `Incorrect payload for references ${JSON.stringify(payload)}`
          )
        }
        r.$hierarchy = payload[k]
        hasKeys = true
      } else if (k === '$noRoot') {
        if (typeof payload[k] !== 'boolean') {
          throw new Error(
            `Incorrect payload type for $noRoot in references ${k}`
          )
        }

        r.$noRoot = payload[k]
        hasKeys = true
        if (field === 'parents') {
          noRoot = payload[k]
        }
      } else if (k === '$_itemCount') {
        // ignore this internal field if setting with a split payload
      } else {
        throw new Error(`Incorrect key for references ${k}`)
      }
    }

    if (hasKeys) {
      result.push(
        '5',
        field,
        createRecord(setRecordDefCstring, {
          op_set_type: OPT_SET_TYPE.reference,
          constraint_id: fields.bidirectional ? 2 : 0,
          delete_all:
            r.delete_all || (!r.$add && !r.$delete && isEmpty(r.$value)),
          $add: await toCArr(
            client,
            field,
            fields,
            schema,
            result,
            r.$add,
            noRoot,
            $lang
          ),
          $delete: await toCArr(
            client,
            field,
            fields,
            schema,
            result,
            r.$delete,
            noRoot,
            $lang
          ),
          $value: await toCArr(
            client,
            field,
            fields,
            schema,
            result,
            r.$value,
            noRoot,
            $lang
          ),
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

    if (
      client.validator &&
      !client.validator(schema, type, field.split('.'), payload, $lang)
    ) {
      throw new Error('Invalid field "references" from custom validator')
    }

    const $value = await toCArr(
      client,
      field,
      fields,
      schema,
      result,
      r,
      noRoot,
      $lang
    )

    result.push(
      '5',
      field,
      createRecord(setRecordDefCstring, {
        op_set_type: OPT_SET_TYPE.reference,
        constraint_id: fields.bidirectional ? 2 : 0,
        delete_all: isEmpty($value),
        $add: '',
        $delete: '',
        $value,
      })
    )
  }

  return 1
}
