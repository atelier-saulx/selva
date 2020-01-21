export type Id = string

export type TypesDb = { idSize: number } & { [key: string]: string }

export type FieldType =
  | 'float'
  | 'number'
  | 'int'
  // | 'json'
  // | 'array'
  | 'references'
  // | 'set'
  | 'string'
  // | 'object'
  | 'text'
  | 'id'
  | 'digest'
  | 'timestamp'
  | 'url'
  | 'email'
  | 'phone'
  | 'geo'
  | 'type'

export type SearchSchema = Record<string, string[]>

export type SearchIndexes = Record<string, SearchSchema>

export type Search = {
  index?: string
  type: ('TAG' | 'TEXT' | 'NUMERIC' | 'SORTABLE')[]
}

export type FieldSchemaObject = {
  type: 'object'
  properties: {
    [key: string]: FieldSchema
  }
}

export type FieldSchemaJson = {
  type: 'json'
  properties?: {
    [key: string]: FieldSchema
  }
  search?: Search
}

export type FieldSchemaOther = {
  search?: Search
  type: FieldType
}

export type FieldSchemaArrayLike = {
  search?: { index?: string; type: 'TAG'[] }
  type: 'set' | 'array'
  // cannot have nested object - needs to be json for set
  items: FieldSchema
}

export type FieldSchema =
  | FieldSchemaObject
  | FieldSchemaJson
  | FieldSchemaArrayLike
  | FieldSchemaOther

export type Fields = Record<string, FieldSchema>

export type TypeSchema = {
  prefix?: string
  hierarchy?:
    | false // has to be false but does not work...
    | {
        [key: string]:
          | false // has to be false but does not work...
          | { excludeAncestryWith: string[] }
          | { includeAncestryWith: string[] }
      }
  fields?: Fields
}

export type Types = Record<string, TypeSchema>

export type Schema = {
  languages?: string[]
  types?: Types
}
