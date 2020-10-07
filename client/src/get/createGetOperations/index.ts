import { SelvaClient } from '../../'
import { GetOperation, GetOptions } from '../types'

import find from './find'
import list from './list'
import all from './all'

export default function createGetOperations(
  client: SelvaClient,
  props: GetOptions,
  id: string,
  field: string,
  db: string,
  ops: GetOperation[] = []
): GetOperation[] {
  if (props.$value) {
    ops.push({
      type: 'value',
      field: field.substr(1),
      value: props.$value
    })
  } else if (props.$id && field) {
    ops.push({
      type: 'nested_query',
      field: field.substr(1),
      props
    })
  } else if (Array.isArray(props)) {
    ops.push({
      type: 'array_query',
      id,
      field: field.substr(1),
      props
    })
  } else if (props.$list) {
    ops.push(list(props, id, field))
  } else if (props.$find) {
    ops.push(find(props.$find, props, id, field, true))
  } else if (props.$field) {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: <string[]>props.$field
    })
  } else if (props.$all) {
    all(client, props, field, id, db, ops)
  } else if (props.$default) {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: field.substr(1),
      default: props.$default
    })
  } else if (typeof props === 'object') {
    for (const key in props) {
      if (key.startsWith('$')) {
        continue
      }
      createGetOperations(client, props[key], id, field + '.' + key, db, ops)
    }
  } else {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: field.substr(1)
    })
  }
  return ops
}
