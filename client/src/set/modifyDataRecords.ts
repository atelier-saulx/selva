import { compile } from 'data-record'

export const doubleDef = compile([{ name: 'd', type: 'double' }])

export const longLongDef = compile([{ name: 'd', type: 'uint64' }])

export const OPT_SET_TYPE = {
  char: 0,
  reference: 1,
  double: 2,
  long_long: 3,
}

export const setRecordDefCstring = compile([
  { name: 'op_set_type', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  { name: 'constraint_id', type: 'uint16' },
  /* 32 zeroed bytes */
  { name: '$add', type: 'cstring_p' },
  { name: '$delete', type: 'cstring_p' },
  { name: '$value', type: 'cstring_p' },
])

export const setRecordDefDouble = compile([
  { name: 'op_set_type', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  /* 48 zeroed bytes */
  { name: '$add', type: 'double_p' },
  { name: '$delete', type: 'double_p' },
  { name: '$value', type: 'double_p' },
])

export const setRecordDefInt64 = compile([
  { name: 'op_set_type', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  /* 48 zeroed bytes */
  { name: '$add', type: 'int64_p' },
  { name: '$delete', type: 'int64_p' },
  { name: '$value', type: 'int64_p' },
])

export const incrementDef = compile([
  { name: '$default', type: 'int64' },
  { name: '$increment', type: 'int64' },
])

export const incrementDoubleDef = compile([
  { name: '$default', type: 'double' },
  { name: '$increment', type: 'double' },
])
