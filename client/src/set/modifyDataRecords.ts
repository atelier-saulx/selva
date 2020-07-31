import { compile } from 'data-record'

export const setRecordDef = compile([
  { name: 'is_reference', type: 'int8' },
  { name: 'delete_all', type: 'int8' },
  { name: '$add', type: 'cstring_p' },
  { name: '$delete', type: 'cstring_p' },
  { name: '$value', type: 'cstring_p' },
])

export const incrementDef = compile([
  { name: 'index', type: 'int32' },
  { name: '$default', type: 'int32' },
  { name: '$increment', type: 'int32' }
])
