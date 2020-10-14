import { GetOptions, Inherit, GetOperation } from '../types'
import { SelvaClient } from '../..'
import { Schema, FieldSchema } from '../../schema'
import { makeAll } from './all'

function validateTypes(schema: Schema, field: string, types: string[]): void {
  let fst: FieldSchema
  let i = 0

  for (; i < types.length; i++) {
    const f = schema.types[types[i]].fields[field]
    if (f) {
      fst = f
    }

    break
  }

  for (; i < types.length; i++) {
    const f =
      types[i] === 'root'
        ? schema.rootType.fields[field]
        : schema.types[types[i]].fields[field]
    if (!f) {
      continue
    }

    if (f.type !== fst.type) {
      throw new Error(
        `Mismatching types for field type for ${field} while inheriting in the provided set of types ${types.join(
          ', '
        )}`
      )
    }
  }
}

export default function createInheritOperation(
  client: SelvaClient,
  inherit: Inherit,
  props: GetOptions,
  id: string,
  field: string,
  db: string,
  ops: GetOperation[]
): void {
  if (typeof inherit === 'boolean') {
    // TODO: make this nice when we have a custom record type in redis
    throw new Error(
      '$inherit: true is currently not supported in fastmode, please provide a type'
    )
  }

  if (inherit.$item) {
    let realKeys: Record<string, true | string> = {}
    for (const prop in props) {
      if (!prop.startsWith('$')) {
        if (props[prop].$field) {
          realKeys[field + '.' + prop] = <string>props[prop].$field
        } else {
          realKeys[field + '.' + prop] = true
        }
      }
    }

    if (props.$all) {
      const newKeys = makeAll(
        client,
        id,
        field,
        <string>props.$field,
        db,
        props
      )
      realKeys = newKeys || realKeys
    }

    ops.push({
      type: 'inherit',
      id,
      field,
      sourceField: props.$field || field,
      props: realKeys,
      item: true,
      required: Array.isArray(inherit.$required)
        ? inherit.$required
        : (inherit.$required && [inherit.$required]) || undefined,
      types: Array.isArray(inherit.$item) ? inherit.$item : [inherit.$item]
    })

    return
  }

  const types: string[] = Array.isArray(inherit.$type)
    ? inherit.$type
    : [inherit.$type]

  const schema = client.schemas[db]

  validateTypes(schema, field, types)

  let hasKeys = false
  let realKeys: Record<string, true | string> = {}
  for (const prop in props) {
    if (!prop.startsWith('$')) {
      hasKeys = true
      if (props[prop].$field) {
        realKeys[field + '.' + prop] = <string>props[prop].$field
      } else {
        realKeys[field + '.' + prop] = true
      }
    }
  }

  if (props.$all) {
    const newKeys = makeAll(client, id, field, <string>props.$field, db, props)
    if (newKeys) {
      hasKeys = true
    }

    realKeys = newKeys || realKeys
  }

  if (hasKeys) {
    ops.push({
      type: 'inherit',
      id,
      field,
      sourceField: props.$field || field,
      props: realKeys,
      types
    })

    return
  }

  ops.push({
    type: 'inherit',
    id,
    field,
    sourceField: props.$field || field,
    props: { [field]: true },
    types,
    single: true,
    merge: inherit.$merge
  })

  return
}
