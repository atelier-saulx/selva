import { SelvaClient } from '../'
import { SCRIPT } from '../constants'
import {
  Schema,
  TypeSchema,
  FieldSchema,
  SchemaOptions,
  FIELD_TYPES,
  SchemaDelOpts,
  SchemaMutations,
  isDeleteField,
} from '.'
import { ServerSelector } from '../types'
import { wait, validateFieldName } from '../util'
import mutate from './mutate'

const MAX_SCHEMA_UPDATE_RETRIES = 100

// just return whats different

function validateNewFields(obj: TypeSchema | FieldSchema, path: string) {
  // validate array?

  if ((<any>obj).type) {
    const field = <FieldSchema>obj

    if (field.type === 'array') {
      const keys = Object.keys(field)
      for (const k of keys) {
        if (!(k === 'meta' || k === 'type' || k === 'items')) {
          throw new Error(`Wrong field passed for type array on schema (${k}))`)
        }
      }
      if (field.items) {
        validateNewFields(field.items, `${path}.*`)
      }
    } else if (
      field.type === 'object' ||
      (field.type === 'json' && field.properties)
    ) {
      for (const propName in field.properties) {
        // this is fine to throw
        validateFieldName(path, propName)
        validateNewFields(field.properties[propName], `${path}.${propName}`)
      }
    } else if (field.type === 'record') {
      validateNewFields(field.values, `${path}.*`)
    } else {
      if (!FIELD_TYPES.includes(field.type)) {
        throw new Error(
          `Field ${path} has an unsupported field type ${
            field.type
          }, supported types are ${FIELD_TYPES.join(', ')}`
        )
      }
    }
  } else {
    const type = <TypeSchema>obj
    if (type.fields) {
      for (const fieldName in type.fields) {
        validateFieldName(path, fieldName)
        validateNewFields(type.fields[fieldName], fieldName)
      }
    }
  }
}

// here we have to do that it just collects the data of what changed (instead of the error)
// then you get the option to say return whats changed and then you can decide to force it

// think about mutate in between? before updating the schema :/
// lock db while migrating :/

export function newSchemaDefinition(
  oldSchema: Schema,
  newSchema: Schema,
  delOpts: SchemaDelOpts
): { schema: Schema; mutations: SchemaMutations } {
  const mutations: SchemaMutations = []

  const schema: Schema = {
    sha: oldSchema.sha,
    rootType: oldSchema.rootType,
    languages: newLanguages(
      oldSchema.languages || [],
      newSchema.languages || []
    ),
    types: {},
  }

  for (const typeName in oldSchema.types) {
    if (newSchema.types[typeName]) {
      schema.types[typeName] = newTypeDefinition(
        typeName,
        oldSchema.types[typeName],
        newSchema.types[typeName],
        mutations
      )
    } else {
      schema.types[typeName] = oldSchema.types[typeName]
    }
  }

  for (const typeName in newSchema.types) {
    const newType = newSchema.types[typeName]

    if (!oldSchema.types[typeName]) {
      if (
        newType.prefix === 'ro' ||
        (newType.prefix && oldSchema.prefixToTypeMapping[newType.prefix])
      ) {
        throw new Error(
          `Prefix ${newType.prefix} is already in use by type ${
            newType.prefix === 'ro'
              ? 'root'
              : oldSchema.prefixToTypeMapping[newType.prefix]
          }`
        )
      }
      validateNewFields(newType, '')

      schema.types[typeName] = newType
    }
  }

  if (newSchema.rootType) {
    const typeDef = { fields: {}, prefix: 'ro' }
    for (const fieldName in oldSchema.rootType.fields) {
      if (newSchema.rootType.fields && newSchema.rootType.fields[fieldName]) {
        typeDef.fields[fieldName] = newFieldDefinition(
          `root.${fieldName}`,
          oldSchema.rootType.fields[fieldName],
          newSchema.rootType.fields[fieldName],
          'root',
          mutations
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

    schema.rootType = typeDef
  } else {
    schema.rootType = oldSchema.rootType
  }

  // also do types

  for (const t in delOpts.fields) {
    for (const p of delOpts.fields[t]) {
      mutations.push({
        mutation: 'remove_field',
        type: t,
        old: oldSchema.types[t].fields[p[0]],
        path: p,
      })

      if (p.length === 1) {
        delete schema.types[t].fields[p[0]]
      } else {
        let s = schema.types[t].fields[p[0]]
        for (let i = 1; i < p.length - 1; i++) {
          if (i === p.length - 1) {
            // @ts-ignore
            if (s.properties) {
              // @ts-ignore
              delete s.properties[p[i]]
              // @ts-ignore
            } else if (s.values) {
              // @ts-ignore
              delete s.values[p[i]]
            } else {
              break
            }
          } else {
            // @ts-ignore
            if (s.properties) {
              // @ts-ignore
              s = s.properties[p[i]]
              // @ts-ignore
            } else if (s.values) {
              // @ts-ignore
              s = s.values[p[i]]
            } else {
              break
            }
          }
        }
      }
    }
  }

  for (const type of delOpts.types) {
    mutations.push({
      mutation: 'delete_type',
      type: type,
    })
    delete schema.types[type]
  }

  return { schema, mutations }
}

function newLanguages(oldLangs: string[], newLangs: string[]): string[] {
  const langs: Set<string> = new Set()

  if (!Array.isArray(oldLangs)) {
    oldLangs = ['en']
  }
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
  newType: TypeSchema,
  mutations: SchemaMutations
): TypeSchema {
  const typeDef: TypeSchema = {
    fields: {},
    prefix: (oldType && oldType.prefix) || (newType && newType.prefix),
    hierarchy: (newType && newType.hierarchy) || (oldType && oldType.hierarchy),
  }

  // FIXME: I think this code is dead code
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

  if (newType.meta) {
    typeDef.meta = newType.meta
  } else if (oldType.meta) {
    typeDef.meta = oldType.meta
  }

  for (const fieldName in oldType.fields) {
    if (newType.fields && newType.fields[fieldName]) {
      typeDef.fields[fieldName] = newFieldDefinition(
        `${fieldName}`,
        oldType.fields[fieldName],
        newType.fields[fieldName],
        typeName,
        mutations
      )
    } else {
      typeDef.fields[fieldName] = oldType.fields[fieldName]
    }
  }

  for (const fieldName in newType.fields) {
    if (oldType.fields && !oldType.fields[fieldName]) {
      const newField = newType.fields[fieldName]
      validateNewFields(newField, fieldName)
      typeDef.fields[fieldName] = newField
    }
  }

  return typeDef
}

function newFieldDefinition(
  fieldPath: string,
  oldField: FieldSchema,
  newField: FieldSchema,
  typeName: string,
  mutations: SchemaMutations
): FieldSchema {
  // herew we want to return
  if (isDeleteField(newField)) {
    return
  }

  if (oldField.type !== newField.type) {
    mutations.push({
      mutation: 'change_field',
      type: typeName, // add type
      path: fieldPath.split('.'),
      old: oldField,
      new: newField,
    })
  }

  if (newField && newField.type === 'array') {
    const keys = Object.keys(newField)
    for (const k of keys) {
      if (!(k === 'meta' || k === 'type' || k === 'items')) {
        throw new Error(`Wrong field passed for type array on schema (${k}))`)
      }
    }
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
          (<any>newField).properties[fieldName],
          typeName,
          mutations
        )
      } else {
        props[fieldName] = oldField.properties[fieldName]
      }
    }

    for (const fieldName in (<any>newField).properties) {
      if (!oldField.properties[fieldName]) {
        const newProperty = (<any>newField).properties[fieldName]
        validateNewFields(newProperty, fieldPath + '.' + fieldName)
        props[fieldName] = newProperty
      }
    }

    const result = <any>{
      type: newField.type,
      properties: props,
    }

    if (newField.meta) {
      result.meta = newField.meta
    } else if (oldField.meta) {
      result.meta = oldField.meta
    }

    return result
  } else if (
    (oldField.type === 'set' || oldField.type === 'array') &&
    oldField.items &&
    oldField.items.type !== (<any>newField).items.type
  ) {
    // add mutation

    mutations.push({
      mutation: 'change_field',
      path: fieldPath.split('.'),
      type: typeName,
      old: oldField,
      new: newField,
    })
  }

  if (!newField.meta && oldField.meta) {
    newField.meta = oldField.meta
  }

  return newField
}

export async function updateSchema(
  client: SelvaClient,
  props: SchemaOptions,
  selector: ServerSelector,
  delOpts: SchemaDelOpts,
  allowMutations: boolean = false,
  handleMutations?: (old: { [field: string]: any }) => {
    [field: string]: any
  } | null,
  retry?: number
): Promise<SchemaMutations> {
  retry = retry || 0

  if (!props.types) {
    props.types = {}
  }

  const oldSchema = (await client.getSchema(selector.name)).schema

  const { schema: newSchema, mutations } = newSchemaDefinition(
    oldSchema,
    <Schema>props,
    delOpts
  )

  if (!allowMutations && mutations.length) {
    let str = ''
    for (const mutation of mutations) {
      if (mutation.mutation === 'remove_field') {
        str += `\n    Remove "${mutation.type}", field "${mutation.path.join(
          '.'
        )}" from ${mutation.old.type}`
      } else if (mutation.mutation === 'delete_type') {
        str += `\n    Delete type "${mutation.type}`
      } else {
        str += `\n    Change "${mutation.type}", field "${mutation.path.join(
          '.'
        )}" from ${mutation.old.type} to ${mutation.new.type}`
      }
    }
    throw new Error(
      `Update schema got ${mutations.length} changed field${
        mutations.length > 1 ? 's' : ''
      }${str}`
    )
  }

  try {
    const updated = await client.redis.evalsha(
      selector,
      `${SCRIPT}:update-schema`, // TODO: or should we just evaluate the sha here. maybe not if it's not connected yet? ... we can also just re-queue it
      0,
      `${client.loglevel}:${client.uuid}`,
      // array where you have an option to NOT fail
      JSON.stringify({ schema: newSchema, allowMutations })
    )

    if (updated) {
      const schema = JSON.parse(updated)

      client.schemas[selector.name] = schema
    }
  } catch (e) {
    if (
      e.stack.includes(
        'SHA mismatch: trying to update an older schema version, please re-fetch and try again'
      )
    ) {
      if (retry >= MAX_SCHEMA_UPDATE_RETRIES) {
        throw new Error(
          `Unable to update schema after ${MAX_SCHEMA_UPDATE_RETRIES} attempts`
        )
      }
      await wait(retry * 200)
      await updateSchema(
        client,
        props,
        selector,
        delOpts,
        allowMutations,
        handleMutations,
        retry + 1
      )
    } else {
      if (e.code === 'NR_CLOSED') {
        // canhappen with load and eval script
      } else {
        throw e
      }
    }
  }

  if (allowMutations && mutations.length) {
    await mutate(selector.name, client, mutations, handleMutations, oldSchema)
  }

  return mutations
}
