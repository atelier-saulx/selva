import { compile } from 'data-record'
import { NODE_ID_SIZE } from '../util'

export const doubleDef = compile([{ name: 'd', type: 'double_le' }])

export const longLongDef = compile([{ name: 'd', type: 'int64_le' }])

export const OPT_SET_TYPE = {
  char: 0,
  reference: 1,
  double: 2,
  long_long: 3,
}

export const setRecordDefCstring = compile([
  { name: 'op_set_type', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  { name: 'constraint_id', type: 'uint16_le' },
  /* 32 zeroed bytes */
  { name: '$add', type: 'cstring_p' },
  { name: '$delete', type: 'cstring_p' },
  { name: '$value', type: 'cstring_p' },
])

export const setRecordDefDouble = compile([
  { name: 'op_set_type', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  /* 48 zeroed bytes */
  { name: '$add', type: 'double_le_p' },
  { name: '$delete', type: 'double_le_p' },
  { name: '$value', type: 'double_le_p' },
])

export const setRecordDefInt64 = compile([
  { name: 'op_set_type', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  /* 48 zeroed bytes */
  { name: '$add', type: 'int64_le_p' },
  { name: '$delete', type: 'int64_le_p' },
  { name: '$value', type: 'int64_le_p' },
])

export const edgeMetaDef = compile([
  { name: 'op_code', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  { name: 'dst_node_id', type: 'cstring', size: NODE_ID_SIZE },
  { name: 'meta_field_name', type: 'cstring_p' },
  { name: 'meta_field_value', type: 'cstring_p' },
])

export const incrementDef = compile([
  { name: '$default', type: 'int64_le' },
  { name: '$increment', type: 'int64_le' },
])

export const incrementDoubleDef = compile([
  { name: '$default', type: 'double_le' },
  { name: '$increment', type: 'double_le' },
])
