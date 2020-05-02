declare function tonumber(this: void, str: string): number
declare function tostring(this: void, val: any): string
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

declare function next(this: void, table: any, index?: any): any
declare function error(this: void, err: string): never
