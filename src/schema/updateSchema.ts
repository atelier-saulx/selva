import { Schema, TypeSchema, FieldSchema } from '.'

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

    return newSchema
  }

  const schema: Schema = {
    sha: oldSchema.sha,
    languages: newLanguages(oldSchema.languages, newSchema.languages),
    types: {}
  }

  console.log('first result', schema)

  for (const typeName in oldSchema.types) {
    if (newSchema.types[typeName]) {
      schema.types[typeName] = newTypeDefinition(
        typeName,
        oldSchema[typeName],
        newSchema[typeName]
      )
    } else {
      schema.types[typeName] = oldSchema[typeName]
    }
  }

  for (const typeName in newSchema.types) {
    if (!oldSchema.types[typeName]) {
      schema.types[typeName] = newSchema.types[typeName]
    }
  }
  return newSchema
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
    prefix: oldType.prefix,
    hierarchy: newType.hierarchy || oldType.hierarchy
  }

  if (oldType.prefix && newType.prefix && oldType.prefix !== newType.prefix) {
    throw new Error(
      `Type ${typeName} has a changed prefix from ${oldType.prefix} to ${newType.prefix}`
    )
  }

  for (const fieldName in oldType.fields) {
    if (newType.fields[fieldName]) {
      typeDef[fieldName] = newFieldDefinition(
        `${typeName}.${fieldName}`,
        oldType.fields[fieldName],
        newType.fields[fieldName]
      )
    } else {
      typeDef[fieldName] = oldType.fields[fieldName]
    }
  }

  for (const fieldName in newType.fields) {
    if (!oldType.fields[fieldName]) {
      typeDef.fields[fieldName] = newType.fields[fieldName]
    }
  }

  return typeDef
}

function newFieldDefinition(
  fieldPath: string,
  oldField: FieldSchema,
  newField: FieldSchema
) {
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

    return { type: newField.type, properties: props }
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

  return newField
}
