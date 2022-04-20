import { FieldSchema } from './types'

export const defaultFields: Record<string, FieldSchema> = {
  id: {
    type: 'id',
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
  createdAt: {
    type: 'timestamp',
  },
  updatedAt: {
    type: 'timestamp',
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
  createdAt: {
    type: 'timestamp',
  },
  updatedAt: {
    type: 'timestamp',
  },
}
