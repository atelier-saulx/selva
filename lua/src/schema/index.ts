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
import { objectAssign } from '../util'
import * as logger from '../logger'

export function getSchema(): Schema {
  const schemaStr = r.hget('___selva_schema', 'types')
  if (!schemaStr) {
    return {
      idSeedCounter: 0,
      types: {},
      languages: []
    }
  }

  return cjson.decode(schemaStr)
}

export function getSearchIndexes(): SearchIndexes {
  const searchIndexStr = r.hget('___selva_schema', 'searchIndexes')
  if (!searchIndexStr) {
    return {}
  }

  return cjson.decode(searchIndexStr)
}

function savePrefixMap(schema: Schema): string {
  const prefixMap: Record<string, string> = {}
  for (const typeName in schema.types) {
    prefixMap[<string>schema.types[typeName].prefix] = typeName
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

  let encoded = cjson.encode(schema)
  const sha = redis.sha1hex(encoded)
  schema.sha = sha
  encoded = cjson.encode(schema)
  r.hset('___selva_schema', 'types', encoded)
  return encoded
}

export function saveSearchIndexes(searchIndexes: SearchIndexes): string {
  const encoded = cjson.encode(searchIndexes)
  r.hset('___selva_schema', 'searchIndexes', encoded) // TODO: is this where we actually want to set it?
  return encoded
}

function verifyLanguages(oldSchema: Schema, newSchema: Schema): string | null {
  if (oldSchema.languages === undefined && newSchema.languages === undefined) {
    return null
  } else if (newSchema.languages === undefined) {
    return `Languages missing from the new schema ${cjson.encode(
      oldSchema.languages
    )}`
  }

  if (oldSchema.languages) {
    for (const lang of oldSchema.languages) {
      // check if found in new schema
      for (const newSchemaLang of newSchema.languages) {
        if (newSchemaLang === lang) {
          return null
        }
      }

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
  oldField: FieldSchema,
  newField: FieldSchema
): string | null {
  if (!oldField) {
    return null
  } else if (!newField) {
    return `New schema missing field ${path} for type ${type}`
  }

  if (oldField.type !== newField.type) {
    return `Cannot change existing type for ${type} field ${path} changing from ${oldField.type} to ${newField.type}`
  }

  if (newField.type !== 'object' && newField.type !== 'set') {
    const index = (newField.search && newField.search.index) || 'default'
    if (
      !newField.search ||
      searchChanged(newField.search, (<any>oldField).search) // they are actually the same type, casting
    ) {
      logger.info(
        `Updating search for index ${index}: ${cjson.encode(newField.search)}`
      )
      searchIndexes[index] = searchIndexes[index] || {}
      searchIndexes[index][path] =
        (newField.search && newField.search.type) ||
        ((<any>oldField).search && (<any>oldField).search.type)
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
  for (const field in newType.fields) {
    if (!oldType.fields || !oldType.fields[field]) {
      findSearchConfigurations(
        newType.fields[field],
        field,
        searchIndexes,
        changedIndexes
      )
    } else {
      const err = checkField(
        type,
        field,
        searchIndexes,
        changedIndexes,
        oldType.fields[field],
        newType.fields[field]
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

    logger.info(`matching schemas for type ${type}`)
    // make sure that we're not changing type schemas that already exist
    // Note: prefix equality is verified in ensurePrefixes()
    const err = checkNestedChanges(
      type,
      oldSchema.types[type],
      newSchema.types[type],
      searchIndexes,
      changedSearchIndexes
    )

    if (err) {
      return err
    }
    // Note: hierarchies need not be the same, they can be overwritten
  }

  for (const type in newSchema.types) {
    if (newSchema.types[type] && oldSchema.types[type]) {
      logger.info(`matching schemas for type ${type}`)
      // make sure that we're not changing type schemas that already exist
      // Note: prefix equality is verified in ensurePrefixes()
      const err = checkNestedChanges(
        type,
        oldSchema.types[type],
        newSchema.types[type],
        searchIndexes,
        changedSearchIndexes
      )

      if (err) {
        return err
      }
      // Note: hierarchies need not be the same, they can be overwritten
    } else {
      logger.info(
        `new type  ${type}: ${cjson.encode(newSchema.types[type].fields)}`
      )
      newSchema.types[type].fields = objectAssign(
        {},
        defaultFields,
        newSchema.types[type].fields || {}
      )
      logger.info(
        `new type with default fileds ${type}: ${cjson.encode(
          newSchema.types[type].fields
        )}`
      )
      findSearchConfigurations(
        newSchema.types[type],
        '',
        searchIndexes,
        changedSearchIndexes
      )
    }
  }

  return null
}

function findSearchConfigurations(
  obj: TypeSchema | FieldSchema,
  path: string,
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>
): void {
  logger.info(`findSearchConfigurations`)
  if ((<any>obj).type) {
    const field = <FieldSchema>obj
    if (
      field.type === 'object' ||
      (field.type === 'json' && field.properties)
    ) {
      for (const propName in field.properties) {
        findSearchConfigurations(
          field.properties[propName],
          `${path}.${propName}`,
          searchIndexes,
          changedSearchIndexes
        )
      }
    } else {
      if (field.search) {
        logger.info(
          `Found search configuration in ${path}: ${cjson.encode(field.search)}`
        )
        const index = field.search.index || 'default'
        searchIndexes[index] = searchIndexes[index] || {}
        searchIndexes[index][path] = field.search.type
        changedSearchIndexes[index] = true
      }
    }
  } else {
    const type = <TypeSchema>obj
    if (type.fields) {
      for (const fieldName in type.fields) {
        findSearchConfigurations(
          type.fields[fieldName],
          fieldName,
          searchIndexes,
          changedSearchIndexes
        )
      }
    }
  }
}

export function verifyAndEnsureRequiredFields(
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>,
  oldSchema: Schema,
  newSchema: Schema
): string | null {
  let err: string | null

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

export function updateSchema(
  newSchema: Schema
): [string | null, string | null] {
  logger.info('yesh')
  const changedSearchIndexes: Record<string, boolean> = {}
  const oldSchema = getSchema() || { types: {} }
  if (oldSchema.sha && newSchema.sha !== oldSchema.sha) {
    return [
      null,
      'SHA mismatch: trying to update an older schema version, please re-fetch and try again'
    ]
  }

  const searchIndexes = getSearchIndexes()

  const err = verifyAndEnsureRequiredFields(
    searchIndexes,
    changedSearchIndexes,
    oldSchema,
    newSchema
  )
  if (err) {
    return [null, err]
  }

  updateSearchIndexes(changedSearchIndexes, searchIndexes)
  updateHierarchies(oldSchema, newSchema)
  const saved = saveSchema(newSchema, searchIndexes)
  return [saved, null]
}
