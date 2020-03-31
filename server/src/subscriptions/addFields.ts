import { GetOptions, Schema, FieldSchema } from '@saulx/selva'
import { isObjectLike } from './util'
import { Subscription, Fields, RefsById } from './'

function makeAll(path: string, schema: Schema, opts: GetOptions): GetOptions {
  const newOpts: GetOptions = { ...opts }
  delete newOpts.$all

  const parts = path.split('.')
  if (!newOpts.$id) {
    return newOpts
  }

  const typeName = schema.prefixToTypeMapping[newOpts.$id.substr(0, 2)]
  const type = schema.types[typeName]
  if (!type) {
    return newOpts
  }

  let prop: FieldSchema = {
    type: 'object',
    properties: type.fields
  }

  for (let i = 0; i < parts.length; i++) {
    if (!parts[i]) {
      break
    }

    if (!isObjectLike(prop)) {
      break
    } else {
      prop = prop.properties[parts[i]]
    }
  }

  if (isObjectLike(prop)) {
    for (const propName in prop.properties) {
      newOpts[propName] = true
    }
  } else if (prop.type === 'text') {
    for (const lang of schema.languages) {
      newOpts[lang] = true
    }
  }

  return newOpts
}

function addFields(
  path: string,
  fields: Set<string>,
  schema: Schema,
  opts: GetOptions
): void {
  let hasKeys = false
  for (const key in opts) {
    if (key[0] === '$') {
      if (key === '$all') {
        addFields(path, fields, schema, makeAll(path, schema, opts))
        return
      } else if (key === '$inherit') {
        // TODO: (jim): mystical subscription things
        // have to fix query again
        fields.add('.ancestors')
        return
      } else if (key === '$field') {
        if (Array.isArray(opts.$field)) {
          opts.$field.forEach(f => fields.add('.' + f))
        } else {
          fields.add('.' + opts.$field)
        }
        return
      }
      // FIXME: other special options missing? -- $ref needs to be handled on lua side
      continue
    }

    hasKeys = true

    if (opts[key] === true) {
      fields.add(`${path}.${key}`)
    } else if (typeof opts[key] === 'object') {
      addFields(`${path}.${key}`, fields, schema, opts[key])
    }
  }

  // default to adding the field if only options are specified
  if (!hasKeys) {
    fields.add(path)
  }
}

function addFieldsToSubscription(
  subscription: Subscription,
  fieldMap: Fields,
  schema: Schema,
  channel: string,
  refsById: RefsById
) {
  // add fields directly to subscription

  const { fields, get } = subscription
  // subscriptionsByField
  addFields('', fields, schema, get)
  for (const field of fields) {
    let current = fieldMap[get.$id + field]
    if (!current) {
      fieldMap[get.$id + field] = current = new Set()
    }
    current.add(channel)
  }

  // remove just check if its empty

  if (refsById[get.$id]) {
    for (const refSource in refsById[get.$id]) {
      let current = fieldMap[get.$id + '.' + refSource]
      if (!current) {
        fieldMap[get.$id + '.' + refSource] = current = new Set()
      }
      current.add(channel)
    }
  }
}

function removeFieldsFromSubscription(
  subscription: Subscription,
  fieldMap: Fields,
  channel: string,
  refsById: RefsById
) {
  // add fields directly to subscription

  const { fields, get } = subscription

  for (const field of fields) {
    const current = fieldMap[get.$id + field]
    if (current) {
      current.delete(channel)
      if (current.size === 0) {
        delete fieldMap[get.$id + field]
      }
    }
  }

  if (refsById[get.$id]) {
    for (const refSource in refsById[get.$id]) {
      const current = fieldMap[get.$id + '.' + refSource]
      if (current) {
        current.delete(channel)
        if (current.size === 0) {
          delete fieldMap[get.$id + '.' + refSource]
        }
      }
    }
  }
}

// this will become very annoying....
// function removeFields

export { addFieldsToSubscription, removeFieldsFromSubscription }
