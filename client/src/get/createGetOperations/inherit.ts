import { GetOptions, Inherit, GetOperation } from '../types'
import { SelvaClient } from '../..'
import { Schema, FieldSchema } from '../../schema'
import { makeAll } from './all'

function validateTypes(schema: Schema, field: string, types: string[]): void {
  let fst: FieldSchema
  let i = 0

  // eslint-disable-next-line
  for (; i < types.length; i++) {
    const f = (types[i] === 'root' ? schema.rootType : schema.types[types[i]])
      .fields[field]
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
  ops: GetOperation[],
  passedOnSchema?: Schema
): void {
  if (typeof inherit === 'object' && inherit.$item) {
    let p: GetOptions = props
    if (props.$all) {
      const newKeys = makeAll(
        client,
        id,
        field,
        <string>props.$field,
        db,
        props,
        passedOnSchema
      )
      p = newKeys || props
    }
    ops.push({
      type: 'inherit',
      id,
      field,
      sourceField: props.$field || field,
      props: p,
      item: true,
      required: Array.isArray(inherit.$required)
        ? inherit.$required
        : (inherit.$required && [inherit.$required]) || undefined,
      types: Array.isArray(inherit.$item) ? inherit.$item : [inherit.$item],
    })
    return
  }

  const types: string[] =
    typeof inherit === 'boolean'
      ? []
      : Array.isArray(inherit.$type)
      ? inherit.$type
      : !inherit.$type
      ? []
      : [inherit.$type]

  const schema = passedOnSchema || client.schemas[db]

  if (types && types.length) {
    validateTypes(schema, field, types)
  }

  let hasKeys = false
  let p = props
  if (types.length && props.$all) {
    const newKeys = makeAll(
      client,
      id,
      field,
      <string>props.$field,
      db,
      props,
      passedOnSchema
    )
    if (newKeys) {
      hasKeys = true
    }

    p = newKeys || props
  }

  for (const k in props) {
    if (!k.startsWith('$')) {
      hasKeys = true
      break
    }
  }

  if (hasKeys) {
    ops.push({
      type: 'inherit',
      id,
      field,
      sourceField: props.$field || field,
      props: p,
      types,
    })

    return
  }

  if (typeof inherit === 'boolean') {
    ops.push({
      type: 'inherit',
      id,
      field,
      sourceField: props.$field || field,
      props,
      item: false,
      required: undefined,
      types: [],
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
    merge: inherit.$merge,
    deepMerge: inherit.$deepMerge,
  })
}
