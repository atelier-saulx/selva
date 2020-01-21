export type Id = string | number

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

export type SearchSchema = {
  [key: string]: string[]
}

export type FieldSchemaObject = {
  type: 'object'
  properties: {
    [key: string]: FieldSchema
  }
}

export type FieldSchemaArrayLike = {
  type: 'set' | 'array'
  item: FieldSchema
}

export type FieldSchemaOther = {
  search?: { index?: string; type: ('TAG' | 'TEXT' | 'NUMERIC' | 'SORTABLE')[] }
  type: FieldType
}

export type FieldSchema =
  | FieldSchemaObject
  | FieldSchemaObject
  | FieldSchemaArrayLike

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
