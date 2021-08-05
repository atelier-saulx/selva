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
  return (
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
      const timeseriesCtx = {
        nodeType: type,
        field,
        fieldSchema: fields,
        payload,
      }

      console.log('HELLO TIMESERIES', JSON.stringify(timeseriesCtx, null, 2))
      client.redis.lpush(
        { name: 'timeseries' },
        'timeseries_inserts',
        JSON.stringify(timeseriesCtx)
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
