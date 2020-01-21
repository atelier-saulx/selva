export type FieldType =
  | 'float'
  | 'number'
  | 'int'
  | 'json'
  | 'array'
  | 'references'
  | 'set'
  | 'string'
  | 'object'
  | 'text'
  | 'id'
  | 'digest'
  | 'timestamp'
  | 'url'
  | 'email'
  | 'phone'
  | 'geo'
  | 'type'

export type FieldSchema = {
  search?: { index?: string; type: ('TAG' | 'TEXT' | 'NUMERIC' | 'SORTABLE')[] }
  type: FieldType
  properties?: {
    [key: string]: FieldSchema
  }
}

export type TypeSchema = {
  hierarchy?:
    | false
    | {
        [key: string]:
          | false
          | { excludeAncestryWith: string[] }
          | { includeAncestryWith: string[] }
      }
  fields?: {
    [key: string]: FieldSchema
  }
}

export type Schema = {
  languages?: string[]
  types?: {
    [key: string]: TypeSchema
  }
}
