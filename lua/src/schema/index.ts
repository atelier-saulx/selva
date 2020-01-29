import {
  Schema,
  TypeSchema,
  FieldSchema,
  SearchIndexes,
  Search,
  defaultFields
} from '../../../src/schema/index'
import ensurePrefixes from './prefixes'
import updateSearchIndexes from './searchIndexes'
import updateHierarchies from './hierarchies'
import * as r from '../redis'
import { isEqual } from '../util'

export function getSchema(): Schema {
  return cjson.decode(r.hget('___selva_schema', 'types'))
}

export function getSearchIndexes(): SearchIndexes {
  return cjson.decode(r.hget('___selva_schema', 'searchIndexes'))
}

function savePrefixMap(schema: Schema): string {
  const prefixMap: Record<string, string> = {}
  for (const typeName in schema.types) {
    prefixMap[schema.types[typeName].prefix] = typeName
  }

  const encoded = cjson.encode(prefixMap)
  r.hset('___selva_schema', 'prefixes', encoded)
  return encoded
}

export function saveSchema(
  schema: Schema,
  searchIndexes?: SearchIndexes
): string {
  if (searchIndexes) {
    // FIXME: should we include this in the SHA1?
    saveSearchIndexes(searchIndexes)
  }

  savePrefixMap(schema)

  const encoded = cjson.encode(schema)
  const sha = redis.sha1hex(encoded)
  schema.sha = sha
  r.hset('___selva_schema', 'types', encoded) // TODO: is this where we actually want to set it?
  return encoded
}

export function saveSearchIndexes(searchIndexes: SearchIndexes): string {
  const encoded = cjson.encode(searchIndexes)
  r.hset('___selva_schema', 'searchIndexes', encoded) // TODO: is this where we actually want to set it?
  return encoded
}

function verifyLanguages(oldSchema: Schema, newSchema: Schema): string | null {
  for (const lang of oldSchema.languages) {
    if (newSchema.languages.indexOf(lang) === -1) {
      return `New schema definition missing existing language option ${lang}`
    }
  }

  return null
}

const searchChanged = (newSearch: Search, oldSearch: Search): boolean => {
  if (newSearch.index !== oldSearch.index) {
    return true
  }
  if (newSearch.type.length !== oldSearch.type.length) {
    return true
  }
  for (let i = 0; i < newSearch.type.length; i++) {
    if (newSearch.type[i] !== oldSearch.type[i]) {
      return true
    }
  }
  return false
}

function checkField(
  type: string,
  path: string,
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>,
  oldField: FieldSchema | null,
  newField: FieldSchema
): string | null {
  if (oldField.type !== newField.type) {
    return `Cannot change existing type for ${type} field ${path} changing from ${oldField.type} to ${newField.type}`
  }

  if (newField.type !== 'object' && newField.type !== 'set') {
    const index = newField.search.index || 'default'
    if (
      newField.search ||
      searchChanged(newField.search, (<any>oldField).search) // they are actually the same type, casting
    ) {
      searchIndexes[index] = searchIndexes[index] || {}
      searchIndexes[index][path] = newField.search.type
      changedSearchIndexes[index] = true
    }
  }

  if (
    newField.type === 'object' ||
    (newField.type === 'json' && newField.properties)
  ) {
    for (const key in newField.properties) {
      checkField(
        type,
        path + '.' + key,
        searchIndexes,
        changedSearchIndexes,
        (<any>oldField).properties[key],
        newField.properties[key]
      )
    }
  } else if (newField.type === 'set' || newField.type === 'array') {
    checkField(
      type,
      path,
      searchIndexes,
      changedSearchIndexes,
      (<any>oldField).items,
      newField.items
    )
  }

  return null
}

function checkNestedChanges(
  type: string,
  oldType: TypeSchema,
  newType: TypeSchema,
  searchIndexes: SearchIndexes,
  changedIndexes: Record<string, boolean>
): null | string {
  for (const field in newType) {
    // ensure default fields are set on new types
    if (!oldType[field]) {
      newType.fields = { ...defaultFields, ...newType.fields }
    } else {
      const err = checkField(
        type,
        field,
        searchIndexes,
        changedIndexes,
        oldType[field],
        newType[field]
      )

      if (err) {
        return err
      }
    }
  }

  return null
}

function verifyTypes(
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>,
  oldSchema: Schema,
  newSchema: Schema
): string | null {
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
        const err = checkNestedChanges(
          type,
          oldSchema[type],
          newSchema[type],
          searchIndexes,
          changedSearchIndexes
        )

        if (err) {
          return err
        }
      }
      // Note: hierarchies need not be the same, they can be overwritten
    } else {
      // TODO: new type
    }
  }

  return null
}

export function verifyAndEnsureRequiredFields(
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>,
  oldSchema: Schema,
  newSchema: Schema
): string | null {
  let err: string

  if ((err = verifyLanguages(oldSchema, newSchema))) {
    return err
  }

  if (
    (err = verifyTypes(
      searchIndexes,
      changedSearchIndexes,
      oldSchema,
      newSchema
    ))
  ) {
    return err
  }

  if ((err = ensurePrefixes(oldSchema, newSchema))) {
    return err
  }

  return null
}

// TODO: handle hset ___selva_schema, prefixes equivalent
// TODO: handle search index changes
export function updateSchema(newSchema: Schema): string | null {
  const changedSearchIndexes: Record<string, boolean> = {}
  const oldSchema = getSchema()
  const searchIndexes = getSearchIndexes()
  const err = verifyAndEnsureRequiredFields(
    searchIndexes,
    changedSearchIndexes,
    oldSchema,
    newSchema
  )
  if (err) {
    return err
  }

  updateSearchIndexes(changedSearchIndexes, searchIndexes)
  updateHierarchies(oldSchema, newSchema)
  saveSchema(newSchema)
  return null
}

