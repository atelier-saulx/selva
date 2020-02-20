import { Schema } from '../../../client/src/schema'
import makeAll from './makeAll'
import { GetOptions } from '../../../client/src/get/types'

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
        // TODO(jim): mystical subscription things
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

export default addFields
