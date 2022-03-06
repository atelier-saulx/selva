import {
  Schema,
  TypeSchema,
  FieldSchema,
  Timeseries,
  defaultFields,
  rootDefaultFields,
  FieldSchemaReferences,
} from '../../../src/schema/index'
import ensurePrefixes from './prefixes'
import updateHierarchies from './hierarchies'
import * as r from '../redis'
import { objectAssign } from '../util'

let CACHED_SCHEMA: Schema | null = null
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

export function saveSchema(schema: Schema, timeseries?: Timeseries): string {
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
    // eslint-disable-next-line
    for (const lang of oldSchema.languages) {
      // check if found in new schema
      for (const newSchemaLang of newSchema.languages) {
        if (newSchemaLang === lang) {
          return null
        }
      }
      // need to fix this kind of stuff with updates
      return `New schema definition missing existing language option ${lang}`
    }
  }

  return null
}

function checkField(
  type: string,
  path: string,
  timeseries: Timeseries,
  oldField: FieldSchema,
  newField: FieldSchema,
  allowMutations: boolean
): string | null {
  if (!oldField) {
    findFieldConfigurations(newField, type, path, timeseries, allowMutations)
    return null
  } else if (!newField && !allowMutations) {
    return `New schema missing field ${path} for type ${type}`
  }

  if (!allowMutations && oldField.type !== newField.type) {
    return `Cannot change existing type for ${type} field ${path} changing from ${oldField.type} to ${newField.type}`
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
          timeseries,
          (<any>oldField).properties[key],
          newField.properties[key],
          allowMutations
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
        timeseries,
        (<any>oldField).items,
        newField.items,
        allowMutations
      ))
    ) {
      return err
    }
  } else if (newField.type === 'reference' || newField.type === 'references') {
    const castedOld = <FieldSchemaReferences>oldField
    if (!newField.bidirectional && castedOld.bidirectional) {
      if (!allowMutations) {
        return `Can not change existing edge directionality for ${path} in type ${type}, changing from ${cjson.encode(
          // @ts-ignore
          oldField.bidirectional
        )} to null}. This will be supported in the future.`
      }
    } else if (newField.bidirectional && castedOld.bidirectional) {
      if (
        newField.bidirectional.fromField !== castedOld.bidirectional.fromField
      ) {
        if (!allowMutations) {
          return `Can not change existing edge directionality for ${path} in type ${type}, changing from ${cjson.encode(
            // @ts-ignore
            oldField.bidirectional
          )} to null}. This will be supported in the future.`
        }
      }
    }
  }
  return null
}

function checkNestedChanges(
  type: string,
  oldType: TypeSchema,
  newType: TypeSchema,
  timeseries: Timeseries,
  allowMutations: boolean
): null | string {
  for (const field in oldType.fields) {
    if (!newType.fields) {
      if (allowMutations) {
        return null
      }
      return `Can not reset fields to empty for type ${type}`
    }
    if (!newType.fields[field]) {
      if (allowMutations) {
        return null
      }
      return `Field ${field} for type ${type} missing in new schema`
    } else {
      const err = checkField(
        type,
        field,
        timeseries,
        oldType.fields[field],
        newType.fields[field],
        allowMutations
      )
      if (err) {
        return err
      }
    }
  }

  for (const field in newType.fields) {
    if (!oldType.fields || !oldType.fields[field]) {
      const f = newType.fields[field]
      findFieldConfigurations(f, type, field, timeseries, allowMutations)
      findBidirectionalReferenceConfigurations(
        newType,
        f,
        field,
        allowMutations
      )
    }
  }

  return null
}

function verifyTypes(
  timeseries: Timeseries,
  oldSchema: Schema,
  newSchema: Schema,
  allowMutations: boolean
): string | null {
  // make sure that new schema has all the old fields and that their nested changes don't change existing fields
  for (const type in oldSchema.types) {
    if (!allowMutations && !newSchema.types[type]) {
      return `New schema definition missing existing type ${type}`
    }

    if (newSchema.types[type]) {
      // make sure that we're not changing type schemas that already exist
      // Note: prefix equality is verified in ensurePrefixes()
      const err = checkNestedChanges(
        type,
        oldSchema.types[type],
        newSchema.types[type],
        timeseries,
        allowMutations
      )

      if (err) {
        return err
      }
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
        timeseries,
        allowMutations
      )
      findBidirectionalReferenceConfigurations(
        newSchema.types[type],
        newSchema.types[type],
        '',
        allowMutations
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
    timeseries,
    allowMutations
  )

  if (err) {
    return err
  }

  return null
}

function findFieldConfigurations(
  obj: TypeSchema | FieldSchema,
  typeName: string,
  path: string,
  timeseries: Timeseries,
  allowMutations: boolean
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

          timeseries,
          allowMutations
        )
      }
    } else {
      // changes actual index
      if (field.timeseries) {
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
          timeseries,
          allowMutations
        )
      }
    }
  }
}

function findBidirectionalReferenceConfigurations(
  type: TypeSchema,
  obj: TypeSchema | FieldSchema,
  path: string,
  allowMutations: boolean
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
          `${path}.${propName}`,
          allowMutations
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
          fieldName,
          allowMutations
        )
      }
    }
  }
}

export function verifyAndEnsureRequiredFields(
  timeseries: Timeseries,
  oldSchema: Schema,
  newSchema: Schema,
  allowMutations: boolean
): string | null {
  let err: string | null

  if ((err = verifyLanguages(oldSchema, newSchema))) {
    return err
  }

  if ((err = ensurePrefixes(oldSchema, newSchema))) {
    return err
  }

  if ((err = verifyTypes(timeseries, oldSchema, newSchema, allowMutations))) {
    return err
  }

  return null
}

function checkLanguageChange(
  oldSchema: Schema,
  newSchema: Schema,
  allowMutations: boolean
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
  }
}

// add 'override' option
export function updateSchema(opts: {
  schema: Schema
  allowMutations: boolean
}): [string | null, string | null] {
  const { schema, allowMutations } = opts

  const oldSchema: Schema = getSchema()
  if (oldSchema.sha && schema.sha !== oldSchema.sha) {
    return [
      null,
      'SHA mismatch: trying to update an older schema version, please re-fetch and try again',
    ]
  }

  const timeseries = getTimeseries()

  const err = verifyAndEnsureRequiredFields(
    timeseries,
    oldSchema,
    schema,
    allowMutations
  )
  if (err) {
    return [null, err]
  }

  checkLanguageChange(oldSchema, schema, allowMutations)
  updateHierarchies(oldSchema, schema)
  const saved = saveSchema(schema, timeseries)
  return [saved, null]
}
