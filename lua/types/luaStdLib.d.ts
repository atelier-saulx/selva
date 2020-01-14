declare function tonumber(this: void, str: string): number
declare function type(
  this: void,
  val: any
):
  | 'nil'
  | 'boolean'
  | 'number'
  | 'string'
  | 'function'
  | 'userdata'
  | 'thread'
  | 'table'
