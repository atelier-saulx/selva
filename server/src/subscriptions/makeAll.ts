import { GetOptions } from '../../../client/src/get/types'
import { Schema, FieldSchema } from '../../../client/src/schema'
import { isObjectLike } from './util'

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

export default makeAll
