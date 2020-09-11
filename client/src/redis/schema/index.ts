import { Types, TypeSchema, FieldSchema } from './types'

export * from './types'

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
    type: 'id'
    // never indexes these - uses in keys
    // search: { index: 'default', type: ['TAG'] }
  },
  type: {
    search: { index: 'default', type: ['TAG'] },
    type: 'type'
  },
  name: {
    search: { index: 'default', type: ['TAG'] },
    type: 'string'
  },
  children: {
    type: 'references'
  },
  parents: {
    type: 'references'
  },
  ancestors: {
    type: 'references',
    search: { index: 'default', type: ['TAG'] }
  },
  descendants: {
    type: 'references'
  },
  aliases: {
    type: 'set',
    items: { type: 'string' }
  }
}

export const rootDefaultFields: Record<string, FieldSchema> = {
  id: {
    type: 'id'
  },
  type: {
    type: 'type'
  },
  children: {
    type: 'references'
  },
  descendants: {
    type: 'references'
  }
}
