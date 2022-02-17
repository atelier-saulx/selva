import { Types, TypeSchema, FieldSchema } from './types'

export * from './types'

export type SchemaMutations = {
  mutation: 'field'
  type: string
  path: string[]
  old: FieldSchema
  new: FieldSchema
}[]

export type SchemaOptions = {
  sha?: string
  languages?: string[]
  types?: Types
  rootType?: Pick<TypeSchema, 'fields'>
  idSeedCounter?: number
  prefixToTypeMapping?: Record<string, string>
}

export const defaultFields: Record<string, FieldSchema> = {
  id: {
    type: 'id',
    // never indexes these - uses in keys
  },
  type: {
    type: 'type',
  },
  name: {
    type: 'string',
  },
  children: {
    type: 'references',
  },
  parents: {
    type: 'references',
  },
  ancestors: {
    type: 'references',
  },
  descendants: {
    type: 'references',
  },
  aliases: {
    type: 'set',
    items: { type: 'string' },
  },
}

export const rootDefaultFields: Record<string, FieldSchema> = {
  id: {
    type: 'id',
  },
  type: {
    type: 'type',
  },
  children: {
    type: 'references',
  },
  descendants: {
    type: 'references',
  },
}
