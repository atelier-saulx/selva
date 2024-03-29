import { Schema, SelvaClient } from '../../'
import { GetOptions } from '..'
import { GetOperation } from '../types'
import { getTypeFromId, getNestedSchema, setNestedResult } from '../utils'
import createGetOperations from './'

export function makeAll(
  client: SelvaClient,
  id: string,
  field: string,
  $field: string | undefined,
  db: string,
  props: GetOptions,
  passedOnSchema?: Schema
): GetOptions {
  const schema = passedOnSchema || client.schemas[db]

  const fieldSchema = getNestedSchema(schema, id, $field || field)
  if (!fieldSchema) {
    return null
  }

  if (fieldSchema.type === 'object') {
    const o: GetOptions = {}
    for (const key in fieldSchema.properties) {
      if (props[key] === false) {
        // do nothing
      } else if ($field) {
        setNestedResult(o, key, { $field: $field + '.' + key })
      } else {
        setNestedResult(o, key, true)
      }
    }

    for (const key in props) {
      if (!fieldSchema.properties[key]) {
        setNestedResult(o, key, props[key])
      }
    }
  } else if (fieldSchema.type === 'record' || fieldSchema.type === 'text') {
    // basically this is the same as: `field: true`
    return null
  }

  return {}
}

// needs async to fetch schema...
const all = (
  client: SelvaClient,
  props: GetOptions,
  id: string,
  field: string,
  db: string,
  ops: GetOperation[] = [],
  passedOnSchema?: Schema
): void => {
  const schema = passedOnSchema || client.schemas[db]
  if (field === '') {
    const type = getTypeFromId(schema, id)
    const typeSchema = type === 'root' ? schema.rootType : schema.types[type]

    if (!typeSchema) {
      return
    }

    for (const key in typeSchema.fields) {
      if (props[key] === undefined) {
        if (
          key !== 'children' &&
          key !== 'parents' &&
          key !== 'ancestors' &&
          key !== 'descendants' &&
          typeSchema.fields[key].type !== 'reference' &&
          typeSchema.fields[key].type !== 'references'
        ) {
          ops.push({
            type: 'db',
            id,
            field: key,
            sourceField: key,
          })
        }
      } else if (props[key] === false) {
        // do nothing
      } else {
        createGetOperations(
          client,
          props[key],
          id,
          field + '.' + key,
          db,
          ops,
          passedOnSchema
        )
      }
    }

    for (const key in props) {
      if (!typeSchema.fields[key]) {
        createGetOperations(
          client,
          props[key],
          id,
          field + '.' + key,
          db,
          ops,
          passedOnSchema
        )
      }
    }
  } else {
    const fieldSchema = getNestedSchema(schema, id, field.slice(1))
    if (!fieldSchema) {
      return
    }

    if (fieldSchema.type === 'object') {
      for (const key in fieldSchema.properties) {
        if (props[key] === undefined) {
          const keySchema = fieldSchema.properties[key]
          if (['reference', 'references'].includes(keySchema.type)) {
            return
          }
          ops.push({
            type: 'db',
            id,
            field: field.slice(1) + '.' + key,
            sourceField: field.slice(1) + '.' + key,
          })
        } else if (props[key] === false) {
          // do nothing
        } else {
          createGetOperations(
            client,
            props[key],
            id,
            field + '.' + key,
            db,
            ops,
            passedOnSchema
          )
        }
      }
    } else if (fieldSchema.type === 'record' || fieldSchema.type === 'text') {
      // basically this is the same as: `field: true`
      ops.push({
        type: 'db',
        id,
        field: field.slice(1),
        sourceField: field.slice(1),
      })
    } else if (fieldSchema.type === 'reference') {
      ops.push({
        type: 'nested_query',
        id,
        field: field.slice(1),
        sourceField: field.slice(1),
        fromReference: true,
        props: Object.assign({}, props, { $all: true }),
      })
    }
  }
}

export default all
