export type Id = string

export type TypesDb = { idSize: number } & { [key: string]: string }

export type GetSchemaResult = {
  schema: Schema
}

// maybe reduce the types here... (e.g. email / phone)
export const FIELD_TYPES = [
  'float',
  'boolean',
  'number',
  'int',
  'string',
  'text',
  'id',
  'digest',
  'url',
  'email',
  'phone',
  'geo',
  'type',
  'timestamp',
  'reference',
  'references',
  'object',
  'record',
  'array',
  'set',
  'json',
  'text',
]

export type FieldType =
  | 'float'
  | 'boolean'
  | 'number'
  | 'int'
  | 'string'
  | 'text'
  | 'id'
  | 'digest'
  | 'url'
  | 'email'
  | 'phone'
  | 'geo'
  | 'type'
  | 'timestamp'

export type TimeSeriesFields = Record<string, FieldSchema>

export type Timeseries = Record<string, TimeSeriesFields> // by type to record of fields that are time series

export type FieldSchemaObject = {
  type: 'object'
  properties: {
    [key: string]: FieldSchema
  }
  meta?: any
  timeseries?: boolean
}

export type FieldSchemaJson = {
  type: 'json'
  properties?: {
    [key: string]: FieldSchema
  }
  meta?: any
  timeseries?: boolean
}

export type FieldSchemaRecord = {
  type: 'record'
  values: FieldSchema
  meta?: any
  timeseries?: boolean
}

export type FieldSchemaReferences = {
  type: 'reference' | 'references'
  bidirectional?: {
    fromField: string
  }
  meta?: any
  timeseries?: boolean
}

export type FieldSchemaOther = {
  type: FieldType
  meta?: any
  timeseries?: boolean
}

export type FieldSchemaArrayLike = {
  type: 'set' | 'array'
  items: FieldSchema
  meta?: any
  timeseries?: boolean
}

export type FieldSchema =
  | FieldSchemaObject
  | FieldSchemaRecord
  | FieldSchemaArrayLike
  | FieldSchemaJson
  | FieldSchemaReferences
  | FieldSchemaOther

export type FieldInputSchemaArrayLike = {
  type: 'set' | 'array'
  items: FieldInputSchema
  meta?: any
  timeseries?: boolean
}

export type FieldInputSchemaRecord = {
  type: 'record'
  values: FieldInputSchema
  meta?: any
  timeseries?: boolean
}

export type FieldInputSchemaObject = {
  type: 'object'
  properties: {
    [key: string]: FieldInputSchema
  }
  meta?: any
  timeseries?: boolean
}

export type FieldInputSchema =
  | FieldSchema
  | FieldInputSchemaArrayLike
  | FieldInputSchemaRecord
  | FieldInputSchemaObject
  | DeleteField

// maybe null?

export type DeleteField = { $delete: true }

export type InputFields = Record<string, FieldInputSchema>

export type InputTypeSchema = {
  prefix?: string
  hierarchy?: HierarchySchema
  fields?: InputFields
  $delete?: true
  meta?: any
}

export type SchemaDelOpts = {
  fields: Record<string, string[][]>
  types: string[]
}

export type InputTypes = { [key: string]: InputTypeSchema }

export type InputSchema = Schema & {
  types: Types | InputTypes
}

export const isDeleteField = (
  fieldSchema: FieldInputSchema
): fieldSchema is DeleteField => {
  return !!(<DeleteField>fieldSchema).$delete
}

export type Fields = Record<string, FieldSchema>

export type HierarchySchema =
  | false // has to be false but does not work...
  | {
      [key: string]:
        | false // has to be false but does not work...
        | { excludeAncestryWith: string[] }
        | { includeAncestryWith: string[] }
    }

export type TypeSchema = {
  prefix?: string
  hierarchy?: HierarchySchema
  fields?: Fields
  meta?: any
}

export type Types = { [key: string]: TypeSchema }

export type Schema = {
  sha?: string
  languages?: string[]
  types: Types
  rootType: Pick<TypeSchema, 'fields' | 'prefix'>
  idSeedCounter?: number
  prefixToTypeMapping?: Record<string, string>
}

export type SchemaOptions = {
  sha?: string
  languages?: string[]
  types?: Types
  rootType: Pick<TypeSchema, 'fields' | 'prefix'>
  idSeedCounter?: number
  prefixToTypeMapping?: Record<string, string>
}
