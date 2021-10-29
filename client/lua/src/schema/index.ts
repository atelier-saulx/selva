import {
  Schema,
  TypeSchema,
  FieldSchema,
  SearchIndexes,
  Timeseries,
  SearchRaw,
  defaultFields,
  rootDefaultFields,
  FieldSchemaReferences,
} from '../../../src/schema/index'
import ensurePrefixes from './prefixes'
import updateSearchIndexes from './searchIndexes'
import updateHierarchies from './hierarchies'
import * as r from '../redis'
import { objectAssign } from '../util'
import * as logger from '../logger'

let CACHED_SCHEMA: Schema | null = null
let CACHED_SEARCH_INDEXES: SearchIndexes | null = null
let CACHED_TIMESERIES: Timeseries | null = null

export function getSchema(): Schema {
  if (CACHED_SCHEMA) {
    return CACHED_SCHEMA
  }

  const schemaStr = r.hget('___selva_schema', 'types')
  if (!schemaStr) {
    return {
      idSeedCounter: 0,
      types: {},
      rootType: { fields: rootDefaultFields, prefix: 'ro' },
      languages: [],
      prefixToTypeMapping: {},
    }
  }

  const schema: Schema = cjson.decode(schemaStr)
  CACHED_SCHEMA = schema
  return schema
}

export function getSearchIndexes(): SearchIndexes {
  if (CACHED_SEARCH_INDEXES) {
    return CACHED_SEARCH_INDEXES
  }

  const searchIndexStr = r.hget('___selva_schema', 'searchIndexes')
  if (!searchIndexStr) {
    return {}
  }

  const searchIndexes: SearchIndexes = cjson.decode(searchIndexStr)
  CACHED_SEARCH_INDEXES = searchIndexes
  return searchIndexes
}

export function getTimeseries(): Timeseries {
  if (CACHED_TIMESERIES) {
    return CACHED_TIMESERIES
  }

  const timeseriesStr = r.hget('___selva_schema', 'timeseries')
  if (!timeseriesStr) {
    return {}
  }

  const timeseries: Timeseries = cjson.decode(timeseriesStr)
  CACHED_TIMESERIES = timeseries
  return timeseries
}

function constructPrefixMap(schema: Schema): void {
  const prefixMap: Record<string, string> = {}
  for (const typeName in schema.types) {
    prefixMap[<string>schema.types[typeName].prefix] = typeName
  }

  schema.prefixToTypeMapping = prefixMap
}

export function saveSchema(
  schema: Schema,
  searchIndexes?: SearchIndexes,
  timeseries?: Timeseries
): string {
  if (searchIndexes) {
    // FIXME: should we include this in the SHA1?
    saveSearchIndexes(searchIndexes)
  }

  if (timeseries) {
    saveTimeseries(timeseries)
  }

  constructPrefixMap(schema)

  let encoded = cjson.encode(schema)
  const sha = redis.sha1hex(encoded)
  schema.sha = sha
  CACHED_SCHEMA = schema
  encoded = cjson.encode(schema)
  r.hset('___selva_schema', 'types', encoded)
  redis.call('publish', '___selva_events:schema_update', 'schema_update')
  return encoded
}

export function saveSearchIndexes(searchIndexes: SearchIndexes): string {
  CACHED_SEARCH_INDEXES = searchIndexes
  const encoded = cjson.encode(searchIndexes)
  r.hset('___selva_schema', 'searchIndexes', encoded)
  return encoded
}

export function saveTimeseries(timeseries: Timeseries): string {
  CACHED_TIMESERIES = timeseries
  const encoded = cjson.encode(timeseries)
  r.hset('___selva_schema', 'timeseries', encoded)
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

function searchTypeChanged(
  a: string[] | undefined,
  b: string[] | undefined
): boolean {
  if (!a || !b) {
    return true
  }

  if (a.length !== b.length) {
    return true
  }

  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      return true
    }
  }

  return false
}

const searchChanged = (
  newSearch: SearchRaw | undefined,
  oldSearch: SearchRaw | undefined
): boolean => {
  if (!newSearch) {
    return false
  }

  if (!oldSearch) {
    return true
  }

  if (newSearch.index !== oldSearch.index) {
    return true
  }
  if (newSearch.type.length !== oldSearch.type.length) {
    return true
  }

  return searchTypeChanged(newSearch.type, oldSearch.type)
}

function checkField(
  type: string,
  path: string,
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>,
  timeseries: Timeseries,
  oldField: FieldSchema,
  newField: FieldSchema
): string | null {
  if (!oldField) {
    findFieldConfigurations(
      newField,
      type,
      path,
      searchIndexes,
      changedSearchIndexes,
      timeseries
    )
    return null
  } else if (!newField) {
    return `New schema missing field ${path} for type ${type}`
  }

  if (oldField.type !== newField.type) {
    return `Cannot change existing type for ${type} field ${path} changing from ${oldField.type} to ${newField.type}`
  }

  if (newField.type !== 'object' && newField.type !== 'set') {
    let searchRaw: SearchRaw | undefined = undefined
    if (newField.search) {
      searchRaw = newField.search = convertSearch(newField)
    }

    const index = (searchRaw && searchRaw.index) || 'default'
    if (
      newField.search &&
      searchChanged(searchRaw, (<any>oldField).search) // they are actually the same type, casting
    ) {
      if (
        // @ts-ignore
        searchTypeChanged(searchRaw.type, oldField.search.type)
      ) {
        // TODO: add support for changing schema types', which means recreating index
        return `Can not change existing search types for ${path} in type ${type}, changing from ${cjson.encode(
          // @ts-ignore
          oldField.search.type
        )} to ${cjson.encode(
          // @ts-ignore
          searchRaw && searchRaw.type
        )}. This will be supported in the future.`
      }

      searchIndexes[index] = searchIndexes[index] || {}
      searchIndexes[index][path] =
        (searchRaw && searchRaw.type) ||
        ((<any>oldField).search && (<any>oldField).search.type)
      changedSearchIndexes[index] = true
    }
  }

  if (
    newField.type === 'object' ||
    (newField.type === 'json' && newField.properties)
  ) {
    for (const key in newField.properties) {
      let err: string | null
      if (
        (err = checkField(
          type,
          path + '.' + key,
          searchIndexes,
          changedSearchIndexes,
          timeseries,
          (<any>oldField).properties[key],
          newField.properties[key]
        ))
      ) {
        return err
      }
    }
  } else if (newField.type === 'set' || newField.type === 'array') {
    let err: string | null
    if (
      (err = checkField(
        type,
        path,
        searchIndexes,
        changedSearchIndexes,
        timeseries,
        (<any>oldField).items,
        newField.items
      ))
    ) {
      return err
    }
  } else if (newField.type === 'reference' || newField.type === 'references') {
    const castedOld = <FieldSchemaReferences>oldField
    if (!newField.bidirectional && castedOld.bidirectional) {
      return `Can not change existing edge directionality for ${path} in type ${type}, changing from ${cjson.encode(
        // @ts-ignore
        oldField.bidirectional
      )} to null}. This will be supported in the future.`
    } else if (newField.bidirectional && castedOld.bidirectional) {
      if (
        newField.bidirectional.fromField !== castedOld.bidirectional.fromField
      ) {
        return `Can not change existing edge directionality for ${path} in type ${type}, changing from ${cjson.encode(
          // @ts-ignore
          oldField.bidirectional
        )} to null}. This will be supported in the future.`
      }
    }
  }

  return null
}

function checkNestedChanges(
  type: string,
  oldType: TypeSchema,
  newType: TypeSchema,
  searchIndexes: SearchIndexes,
  changedIndexes: Record<string, boolean>,
  timeseries: Timeseries
): null | string {
  for (const field in oldType.fields) {
    if (!newType.fields) {
      return `Can not reset fields to empty for type ${type}`
    }

    if (!newType.fields[field]) {
      return `Field ${field} for type ${type} missing in new schema`
    } else {
      const err = checkField(
        type,
        field,
        searchIndexes,
        changedIndexes,
        timeseries,
        oldType.fields[field],
        newType.fields[field]
      )

      if (err) {
        return err
      }
    }
  }

  for (const field in newType.fields) {
    if (!oldType.fields || !oldType.fields[field]) {
      const f = newType.fields[field]
      findFieldConfigurations(
        f,
        type,
        field,
        searchIndexes,
        changedIndexes,
        timeseries
      )
      findBidirectionalReferenceConfigurations(newType, f, field)
    }
  }

  return null
}

function verifyTypes(
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>,
  timeseries: Timeseries,
  oldSchema: Schema,
  newSchema: Schema
): string | null {
  // make sure that new schema has all the old fields and that their nested changes don't change existing fields
  for (const type in oldSchema.types) {
    if (!newSchema.types[type]) {
      return `New schema definition missing existing type ${type}`
    }

    // make sure that we're not changing type schemas that already exist
    // Note: prefix equality is verified in ensurePrefixes()
    const err = checkNestedChanges(
      type,
      oldSchema.types[type],
      newSchema.types[type],
      searchIndexes,
      changedSearchIndexes,
      timeseries
    )

    if (err) {
      return err
    }
    // Note: hierarchies need not be the same, they can be overwritten
  }

  // Ensure default fields on new types
  for (const type in newSchema.types) {
    if (!oldSchema.types[type]) {
      newSchema.types[type].fields = objectAssign(
        {},
        defaultFields,
        newSchema.types[type].fields || {}
      )

      findFieldConfigurations(
        newSchema.types[type],
        type,
        '',
        searchIndexes,
        changedSearchIndexes,
        timeseries
      )
      findBidirectionalReferenceConfigurations(
        newSchema.types[type],
        newSchema.types[type],
        ''
      )
    }
  }

  // crheck root type
  if (!newSchema.rootType) {
    return `New schema definition missing existing type for root (schema.rootType)`
  }

  const err = checkNestedChanges(
    'root',
    oldSchema.rootType,
    newSchema.rootType,
    {}, // skip indexing for root
    {},
    timeseries
  )

  if (err) {
    return err
  }

  return null
}

function convertSearch(f: FieldSchema): SearchRaw {
  const field = <any>f
  if (field.search === true) {
    if (
      field.type === 'json' ||
      field.type === 'string' ||
      field.type === 'array'
    ) {
      return { type: ['TEXT'] }
    } else if (
      field.type === 'number' ||
      field.type === 'float' ||
      field.type === 'int' ||
      field.type === 'timestamp'
    ) {
      return { type: ['NUMERIC', 'SORTABLE'] }
    } else if (field.type === 'boolean') {
      return { type: ['TAG'] }
    } else if (field.type === 'text') {
      return { type: ['TEXT-LANGUAGE'] }
    } else if (field.type === 'geo') {
      return { type: ['GEO'] }
    } else if (field.type === 'set') {
      return { type: ['TAG'] }
    }
  }
  return field.search
}

function findFieldConfigurations(
  obj: TypeSchema | FieldSchema,
  typeName: string,
  path: string,
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>,
  timeseries: Timeseries
): void {
  if ((<any>obj).type) {
    const field = <FieldSchema>obj
    if (
      field.type === 'object' ||
      (field.type === 'json' && field.properties)
    ) {
      for (const propName in field.properties) {
        findFieldConfigurations(
          field.properties[propName],
          typeName,
          `${path}.${propName}`,
          searchIndexes,
          changedSearchIndexes,
          timeseries
        )
      }
    } else {
      // changes actual index
      if (field.search) {
        field.search = convertSearch(field)

        const index = field.search.index || 'default'
        searchIndexes[index] = searchIndexes[index] || {}
        if (searchIndexes[index][path]) {
          if (
            <any>searchIndexes[index][path].length < field.search.type.length
          ) {
            searchIndexes[index][path] = field.search.type
            changedSearchIndexes[index] = true
          } else if (field.search.type[0] !== searchIndexes[index][path][0]) {
            logger.error(
              `Search index for "${path}" type is ${searchIndexes[index][path][0]} want to change to ${field.search.type[0]}`
            )
          }
        } else {
          searchIndexes[index][path] = field.search.type
          changedSearchIndexes[index] = true
        }
      } else if (field.timeseries) {
        if (!timeseries[typeName]) {
          timeseries[typeName] = {}
        }

        const tsFields = timeseries[typeName]
        tsFields[path] = field
      }
    }
  } else {
    const type = <TypeSchema>obj
    if (type.fields) {
      for (const fieldName in type.fields) {
        findFieldConfigurations(
          type.fields[fieldName],
          typeName,
          fieldName,
          searchIndexes,
          changedSearchIndexes,
          timeseries
        )
      }
    }
  }
}

function findBidirectionalReferenceConfigurations(
  type: TypeSchema,
  obj: TypeSchema | FieldSchema,
  path: string
) {
  if ((<any>obj).type) {
    const field = <FieldSchema>obj
    if (
      field.type === 'object' ||
      (field.type === 'json' && field.properties)
    ) {
      for (const propName in field.properties) {
        findBidirectionalReferenceConfigurations(
          type,
          field.properties[propName],
          `${path}.${propName}`
        )
      }
    } else {
      const f = field
      if (f.type === 'reference' && f.bidirectional) {
        // TODO: needs back type?
        redis.call(
          'selva.hierarchy.addconstraint',
          '___selva_hierarchy',
          <string>type.prefix,
          path,
          '3',
          f.bidirectional.fromField
        )
      } else if (f.type === 'references' && f.bidirectional) {
        // TODO: needs back type?
        redis.call(
          'selva.hierarchy.addconstraint',
          '___selva_hierarchy',
          <string>type.prefix,
          path,
          '2',
          f.bidirectional.fromField
        )
      }
    }
  } else {
    const type = <TypeSchema>obj
    if (type.fields) {
      for (const fieldName in type.fields) {
        findBidirectionalReferenceConfigurations(
          type,
          type.fields[fieldName],
          fieldName
        )
      }
    }
  }
}

export function verifyAndEnsureRequiredFields(
  searchIndexes: SearchIndexes,
  changedSearchIndexes: Record<string, boolean>,
  timeseries: Timeseries,
  oldSchema: Schema,
  newSchema: Schema
): string | null {
  let err: string | null

  if ((err = verifyLanguages(oldSchema, newSchema))) {
    return err
  }

  if ((err = ensurePrefixes(oldSchema, newSchema))) {
    return err
  }

  if (
    (err = verifyTypes(
      searchIndexes,
      changedSearchIndexes,
      timeseries,
      oldSchema,
      newSchema
    ))
  ) {
    return err
  }

  return null
}

function checkLanguageChange(
  changedSearchIndexes: Record<string, boolean>,
  searchIndexes: SearchIndexes,
  oldSchema: Schema,
  newSchema: Schema
) {
  if (newSchema.languages) {
    const addedLanguages: string[] = []
    for (let i = 0; i < newSchema.languages.length; i++) {
      const lang = newSchema.languages[i]
      if (oldSchema.languages) {
        let found = false
        for (let i = 0; i < oldSchema.languages.length; i++) {
          if (oldSchema.languages[i] === lang) {
            found = true
            break
          }
        }
        if (!found) {
          addedLanguages[addedLanguages.length] = lang
        }
      } else {
        addedLanguages[addedLanguages.length] = lang
      }
    }
    if (addedLanguages.length !== 0) {
      for (const index in searchIndexes) {
        const searchIndex = searchIndexes[index]
        for (const field in searchIndex) {
          if (searchIndex[field][0] === 'TEXT-LANGUAGE') {
            changedSearchIndexes[index] = true
            break
          }
        }
      }
    }
  }
}

export function updateSchema(
  newSchema: Schema
): [string | null, string | null] {
  const changedSearchIndexes: Record<string, boolean> = {}
  const oldSchema: Schema = getSchema()
  if (oldSchema.sha && newSchema.sha !== oldSchema.sha) {
    return [
      null,
      'SHA mismatch: trying to update an older schema version, please re-fetch and try again',
    ]
  }

  const searchIndexes = getSearchIndexes()
  const timeseries = getTimeseries()

  const err = verifyAndEnsureRequiredFields(
    searchIndexes,
    changedSearchIndexes,
    timeseries,
    oldSchema,
    newSchema
  )
  if (err) {
    return [null, err]
  }

  checkLanguageChange(changedSearchIndexes, searchIndexes, oldSchema, newSchema)
  updateSearchIndexes(changedSearchIndexes, searchIndexes, newSchema)
  updateHierarchies(oldSchema, newSchema)
  const saved = saveSchema(newSchema, searchIndexes, timeseries)
  return [saved, null]
}
