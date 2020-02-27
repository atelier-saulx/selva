import { Schema, TypeSchema, FieldSchema, rootDefaultFields } from '.'

export function newSchemaDefinition(
  oldSchema: Schema,
  newSchema: Schema
): Schema {
  if (!oldSchema) {
    if (!newSchema.languages) {
      newSchema.languages = []
    }

    if (!newSchema.types) {
      newSchema.types = {}
    }

    newSchema.rootType = {
      fields: {
        ...rootDefaultFields,
        ...((newSchema.rootType && newSchema.rootType.fields) || {})
      }
    }

    return newSchema
  }

  const schema: Schema = {
    sha: oldSchema.sha,
    rootType: oldSchema.rootType,
    languages: newLanguages(
      oldSchema.languages || [],
      newSchema.languages || []
    ),
    types: {}
  }

  for (const typeName in oldSchema.types) {
    if (newSchema.types[typeName]) {
      schema.types[typeName] = newTypeDefinition(
        typeName,
        oldSchema.types[typeName],
        newSchema.types[typeName]
      )
    } else {
      schema.types[typeName] = oldSchema.types[typeName]
    }
  }

  for (const typeName in newSchema.types) {
    if (!oldSchema.types[typeName]) {
      schema.types[typeName] = newSchema.types[typeName]
    }
  }

  if (newSchema.rootType) {
    const typeDef = { fields: {} }
    for (const fieldName in oldSchema.rootType.fields) {
      if (newSchema.rootType.fields && newSchema.rootType.fields[fieldName]) {
        typeDef.fields[fieldName] = newFieldDefinition(
          `root.${fieldName}`,
          oldSchema.rootType.fields[fieldName],
          newSchema.rootType.fields[fieldName]
        )
      } else {
        typeDef.fields[fieldName] = oldSchema.rootType.fields[fieldName]
      }
    }

    for (const fieldName in newSchema.rootType.fields) {
      if (oldSchema.rootType.fields && !oldSchema.rootType.fields[fieldName]) {
        typeDef.fields[fieldName] = newSchema.rootType.fields[fieldName]
      }
    }
  } else {
    schema.rootType = oldSchema.rootType
  }

  return schema
}

function newLanguages(oldLangs: string[], newLangs: string[]): string[] {
  const langs: Set<string> = new Set()
  for (const lang of oldLangs) {
    langs.add(lang)
  }

  for (const lang of newLangs) {
    langs.add(lang)
  }

  return [...langs.values()]
}

function newTypeDefinition(
  typeName: string,
  oldType: TypeSchema,
  newType: TypeSchema
): TypeSchema {
  const typeDef: TypeSchema = {
    fields: {},
    prefix: (oldType && oldType.prefix) || (newType && newType.prefix),
    hierarchy: (newType && newType.hierarchy) || (oldType && oldType.hierarchy)
  }

  if (!oldType) {
    return newType
  } else if (!newType) {
    return oldType
  }

  if (oldType.prefix && newType.prefix && oldType.prefix !== newType.prefix) {
    throw new Error(
      `Type ${typeName} has a changed prefix from ${oldType.prefix} to ${newType.prefix}`
    )
  }

  for (const fieldName in oldType.fields) {
    if (newType.fields && newType.fields[fieldName]) {
      typeDef.fields[fieldName] = newFieldDefinition(
        `${typeName}.${fieldName}`,
        oldType.fields[fieldName],
        newType.fields[fieldName]
      )
    } else {
      typeDef.fields[fieldName] = oldType.fields[fieldName]
    }
  }

  for (const fieldName in newType.fields) {
    if (oldType.fields && !oldType.fields[fieldName]) {
      typeDef.fields[fieldName] = newType.fields[fieldName]
    }
  }

  return typeDef
}

function newFieldDefinition(
  fieldPath: string,
  oldField: FieldSchema,
  newField: FieldSchema
): FieldSchema {
  if (oldField.type !== newField.type) {
    throw new Error(
      `Path ${fieldPath} has mismatching types, trying to change ${oldField.type} to ${newField.type}`
    )
  }

  if (
    oldField.type === 'object' ||
    (oldField.type === 'json' && oldField.properties)
  ) {
    const props = {}
    for (const fieldName in oldField.properties) {
      if ((<any>newField).properties[fieldName]) {
        props[fieldName] = newFieldDefinition(
          `${fieldPath}.${fieldName}`,
          oldField.properties[fieldName],
          (<any>newField).properties[fieldName]
        )
      } else {
        props[fieldName] = oldField.properties[fieldName]
      }
    }

    for (const fieldName in (<any>newField).properties) {
      if (!oldField.properties[fieldName]) {
        props[fieldName] = (<any>newField).properties[fieldName]
      }
    }

    const result = <any>{
      type: newField.type,
      properties: props
    }

    if (newField.meta) {
      result.meta = newField.meta
    } else if (oldField.meta) {
      result.meta = oldField.meta
    }

    return result
  } else if (
    (oldField.type === 'set' || oldField.type === 'array') &&
    oldField.items.type !== (<any>newField).items.type
  ) {
    throw new Error(
      `Path ${fieldPath} has mismatching types, trying to change collection with type ${
        oldField.items.type
      } to type ${(<any>newField).items.type}`
    )
  }

  if (!(<any>newField).search) {
    if (oldField.search) {
      ;(<any>newField).search = oldField.search
    }
  }

  if (!newField.meta && oldField.meta) {
    newField.meta = oldField.meta
  }

  return newField
}
