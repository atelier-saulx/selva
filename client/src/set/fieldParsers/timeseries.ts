import { createRecord } from 'data-record'
import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchema } from '../../schema'
import fieldParsers from '.'

import { longLongDef } from '../modifyDataRecords'

export default async (
  client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: (string | Buffer)[],
  fields: FieldSchema,
  type: string,
  $lang?: string
): Promise<number> => {
  if (typeof payload !== 'object' || Array.isArray(payload)) {
    throw new Error(
      `Incorrect payload for timeseries ${JSON.stringify(payload)}`
    )
  }

  const fn = fieldParsers[fields.type]
  const addedFields = fn(
    client,
    schema,
    `${field}._value`,
    payload._value,
    result,
    Object.assign({}, fields, { timeseries: false }),
    type,
    $lang
  )

  if (addedFields) {
    result.push(
      '3',
      `${field}.$selva_timeseries`,
      createRecord(longLongDef, {
        d: BigInt(1),
      })
    )

    result.push(
      '3',
      `${field}._ts`,
      createRecord(longLongDef, {
        d: BigInt(payload._ts),
      })
    )

    // TODO: add timeseries meta
    const content = new Uint32Array([0])
    const buf = Buffer.from(content.buffer)
    result.push('C', field, buf)
  }

  return addedFields
}