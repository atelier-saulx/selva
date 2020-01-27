import { Schema } from '../../../src/schema/index'
import ensurePrefixes from './prefixes'
import * as r from '../redis'

export function getSchema() {
  return cjson.decode(r.get('___selva_schema'))
}

function verifyLanguages(oldSchema: Schema, newSchema: Schema): string | null {
  for (const lang of oldSchema.languages) {
    if (newSchema.languages.indexOf(lang) === -1) {
      return `New schema definition missing existing language option ${lang}`
    }
  }

  return null
}

function verifyTypes(oldSchema: Schema, newSchema: Schema): string | null {
  for (const type in oldSchema.types) {
    if (!newSchema.types[type]) {
      return `New schema definition missing existing type ${type}`
    }
  }

  return null
}

export function verifyAndEnsurePrefixes(
  oldSchema: Schema,
  newSchema: Schema
): string | null {
  let err: string

  if ((err = verifyLanguages(oldSchema, newSchema))) {
    return err
  }

  if ((err = verifyTypes(oldSchema, newSchema))) {
    return err
  }

  if ((err = ensurePrefixes(oldSchema, newSchema))) {
    return err
  }

  return null
}

// TODO: handle hset ___selva_schema, prefixes equivalent
export function saveSchema(schema: Schema): string {
  const encoded = cjson.encode(schema)
  const sha = redis.sha1hex(encoded)
  schema.sha = sha
  r.set('___selva_schema', encoded) // TODO: is this where we actually want to set it?
  return encoded
}

