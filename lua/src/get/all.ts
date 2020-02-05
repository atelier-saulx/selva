import { Id, Schema, FieldSchemaObject } from '~selva/schema/index'
import { GetOptions } from '~selva/get/types'
import { getTypeFromId } from 'lua/src/typeIdMapping'
import { splitString } from 'lua/src/util'
import * as logger from '../logger'

export default function makeNewGetOptions(
  id: Id,
  field: string,
  schema: Schema,
  opts: GetOptions
): GetOptions {
  const newOpts: GetOptions = {}

  // copy over old keys
  for (const key in opts) {
    newOpts[key] = opts[key]
  }
  delete newOpts.$all

  const typeName = getTypeFromId(id)
  const typeSchema = schema.types[typeName]

  const path: string[] = splitString(field, '.')

  if (path.length === 0) {
    for (const field in typeSchema.fields) {
      if (!newOpts[field] && typeSchema.fields[field].type !== 'references') {
        newOpts[field] = true
      }
    }

    return newOpts
  }

  if (!typeSchema.fields) {
    typeSchema.fields = {}
  }

  let prop = typeSchema.fields[path[0]]
  for (let i = 1; i < path.length; i++) {
    if ((<any>prop).properties) {
      const obj = <FieldSchemaObject>prop
      prop = obj.properties[path[i]]
    } else if (prop) {
      break
    }
  }

  if (!prop) {
    return newOpts
  }

  if (prop.type === 'text' && schema.languages) {
    for (const lang of schema.languages) {
      newOpts[lang] = true
    }
  } else if (prop.type === 'object') {
    for (const property in prop.properties) {
      newOpts[property] = true
    }
  } else if (prop.type === 'json' && prop.properties) {
    for (const property in prop.properties) {
      newOpts[property] = true
    }
  }

  return newOpts
}
