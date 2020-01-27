import { Schema } from '../../../src/schema/index'
import ensurePrefixes from './prefixes'
import updateSearchIndexes from './searchIndexes'
import * as r from '../redis'

export function getSchema(): Schema {
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

const isEqual = (a: any, b: any): boolean => {
  const typeA = type(a)
  if (typeA !== type(b)) {
    return false
  }

  if (typeA === 'table') {
    for (let key in a) {
      if (!b[key]) {
        return false
      } else {
        if (!isEqual(a[key], b[key])) {
          return false
        }
      }
    }
  } else if (a !== b) {
    return false
  }
  return true
}

function verifyTypes(oldSchema: Schema, newSchema: Schema): string | null {
  for (const type in oldSchema.types) {
    if (!newSchema.types[type]) {
      return `New schema definition missing existing type ${type}`
    }

    if (newSchema.types[type] && oldSchema.types[type]) {
      // make sure that we're not changing type schemas that already exist
      // Note: prefix equality is verified in ensurePrefixes()
      if (
        !isEqual(oldSchema.types[type].fields, newSchema.types[type].fields)
      ) {
        return `Schemas are not the same for type ${type}, trying to change ${cjson.encode(
          oldSchema.types[type]
        )} to ${cjson.encode(newSchema.types[type])}`
      }
      // Note: hierarchies need not be the same, they can be overwritten
    }
  }

  return null
}

export function verifyAndEnsureRequiredFields(
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

export function saveSchema(schema: Schema): string {
  const encoded = cjson.encode(schema)
  const sha = redis.sha1hex(encoded)
  schema.sha = sha
  r.set('___selva_schema', encoded) // TODO: is this where we actually want to set it?
  return encoded
}

// TODO: handle hset ___selva_schema, prefixes equivalent
// TODO: handle search index changes
export function updateSchema(newSchema: Schema) {
  const oldSchema = getSchema()
  verifyAndEnsureRequiredFields(oldSchema, newSchema)
  updateSearchIndexes() // TODO
  // TODO: update ancestors on hierarchy updates
  saveSchema(newSchema)
}

