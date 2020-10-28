import { SelvaClient } from '../../'
import { GetOperation, GetOptions } from '../types'

import find from './find'
import list from './list'
import all from './all'
import createInheritOperation from './inherit'

import { getNestedSchema } from '../utils'

export default function createGetOperations(
  client: SelvaClient,
  props: GetOptions,
  id: string,
  field: string,
  db: string,
  ops: GetOperation[] = []
): GetOperation[] {
  const schema = client.schemas[db]

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
  } else if (props.$default) {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: props.$field || field.substr(1),
      default: props.$default
    })
  } else if (props.$inherit) {
    createInheritOperation(
      client,
      props.$inherit,
      props,
      id,
      field.slice(1),
      db,
      ops
    )
  } else if (props.$all) {
    if (props.$field) {
      const $field = Array.isArray(props.$field) ? props.$field : [props.$field]
      all(client, props, id, $field[0], db, ops)
    } else {
      all(client, props, id, field, db, ops)
    }
  } else if (props.$field) {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: <string[]>props.$field
    })
  } else if (typeof props === 'object') {
    const fs = getNestedSchema(schema, id, field.substr(1))

    if (fs && fs.type === 'reference') {
      if (props.$flatten) {
        const parts = field.substr(1).split('.')
        const flattened = parts.slice(0, parts.length - 1).join('.')
        ops.push({
          type: 'nested_query',
          field: flattened,
          props,
          id,
          flatten: field.substr(1)
        })
      } else {
        ops.push({
          type: 'nested_query',
          field: field.substr(1),
          props,
          id
        })
      }

      return
    }

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
