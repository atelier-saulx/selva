import { SelvaClient } from '../..'
import { Schema, FieldSchema } from '../../schema'
import { SetOptions } from '../types'
import simple from './simple'
import text from './text'
import geo from './geo'
import references from './references'
import reference from './reference'
import set from './set'
import json from './json'
import object from './object'
import record from './record'
import array from './array'
import timeseries from './timeseries'
import { deepCopy, deepMerge } from '@saulx/utils'
import { getNestedField, setNestedResult } from '../../get/utils'

type FieldParserFn = (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchema,
  type: string,
  $lang?: string
) => Promise<number>

const wrapTimeseries: (fn: FieldParserFn) => FieldParserFn = (
  fn: FieldParserFn
) => {
  return async (
    client: SelvaClient,
    schema: Schema,
    field: string,
    payload: SetOptions,
    result: (string | Buffer)[],
    fields: FieldSchema,
    type: string,
    $lang?: string
  ) => {
    if (fields.timeseries) {
      const ts = Date.now()

      let tsPayload = payload

      if (
        payload.$merge !== false &&
        ['object', 'record'].includes(fields.type)
      ) {
        const getOpts = {
          $id: (<any>result).$id,
          ts: { $raw: `${field}._ts` },
        }
        setNestedResult(getOpts, field, true)

        const prevResult = await client.get(getOpts)
        console.log('PREV RESULT', prevResult)
        const prevValue = getNestedField(prevResult, field) || {}
        tsPayload = deepMerge(prevValue, payload)
      }

      const timeseriesCtx = {
        nodeId: (<any>result).$id,
        nodeType: type,
        field,
        fieldSchema: fields,
        payload: tsPayload,
        ts,
      }

      console.log('LPUSH', timeseriesCtx)
      client.redis.lpush(
        { type: 'timeseriesQueue' },
        'timeseries_inserts',
        JSON.stringify({ type: 'insert', context: timeseriesCtx })
      )

      return timeseries(
        client,
        schema,
        field,
        { $selva_timeseries: true, _value: payload, _ts: ts },
        result,
        fields,
        type,
        $lang
      )
    }

    return fn(client, schema, field, payload, result, fields, type, $lang)
  }
}

const fieldParsers: {
  [index: string]: (
    client: SelvaClient,
    schema: Schema,
    field: string,
    payload: SetOptions,
    result: (string | Buffer)[],
    fields: FieldSchema,
    type: string,
    $lang?: string
  ) => Promise<number>
} = {
  ...simple,
  text,
  geo,
  set,
  reference,
  references,
  json,
  object,
  array,
  record,
}

for (const fnName in fieldParsers) {
  fieldParsers[fnName] = wrapTimeseries(fieldParsers[fnName])
}

export default fieldParsers
