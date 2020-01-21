import { SelvaClient } from '../'

// advanced type

type FieldSchema = {
  search?: { index?: string; type: ('TAG' | 'TEXT' | 'NUMERIC' | 'SORTABLE')[] }
}

type TypeSchema = {}

type SchemaOptions = {
  languages?: string[]
  types?: {
    [key: string]: any
  }
}

async function updateSchema(
  client: SelvaClient,
  props: SchemaOptions
): Promise<boolean> {
  return true
}

export { updateSchema, SchemaOptions, FieldSchema, TypeSchema }
