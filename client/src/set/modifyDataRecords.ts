import { compile } from 'data-record'

export const doubleDef = compile([
  { name: 'd', type: 'double' }
]);

export const longLongDef = compile([
  { name: 'd', type: 'uint64' }
]);

export const setRecordDef = compile([
  { name: 'is_reference', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  { name: '$add', type: 'cstring_p' },
  { name: '$delete', type: 'cstring_p' },
  { name: '$value', type: 'cstring_p' },
])

export const incrementDef = compile([
  { name: '$default', type: 'int64' },
  { name: '$increment', type: 'int64' }
])

export const incrementDoubleDef = compile([
  { name: '$default', type: 'double' },
  { name: '$increment', type: 'double' }
])
