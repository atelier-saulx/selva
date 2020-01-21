import { SelvaClient } from '..'
import { Types, SearchIndexes, FieldSchema } from './'
import updateIndex from './updateIndex'

const parseField = (
  type: string,
  field: FieldSchema,
  searchIndexes: SearchIndexes,
  changedIndexes: string[],
  props: FieldSchema | { [key: string]: FieldSchema },
  path: string[]
  // previous
): boolean => {
  let changed = false
  let segment = props
  for (let i = 0; i < path.length; i++) {
    if (path.length - 1) {
      if (props[path[i]]) {
        // type exists
        if (field.type !== props[path[i]].type) {
          throw new Error(
            `Cannot change existing type for ${type} field ${path} changing from ${
              props[path[i]].type
            } to ${field.type}`
          )
        }
      } else {
        changed = true
        props[path[i]] = {
          type: field.type
        }
      }
    }
    segment = props[path[i]] || {}
  }

  console.log('look -->', type, path, segment)

  if (field.type !== 'object' && field.type !== 'set') {
    if (field.search) {
      // -- do it
    }
  }

  if (field.type === 'object' || field.type === 'json') {
    // not only string allow properties on this
  } else if (field.type === 'set' || field.type === 'array') {
  }

  // if object or json or array or set

  return changed
}

// needs to potentially re-index everythign
const updateHierarchy = async () => {}

async function updateTypeSchema(
  client: SelvaClient,
  props: Types,
  types: Types,
  searchIndexes: SearchIndexes
): Promise<void> {
  const changedTypes: string[] = []
  const changedIndexes: string[] = []

  for (const type in props) {
    if (types[type]) {
      console.log('TYPE exists tricky!', type)
    }
    let changed: boolean = false
    const fields = props[type].fields
    types[type] = {
      fields: {}
    }
    if (fields) {
      for (let field in fields) {
        types[type].fields[field] = {
          type: fields[field].type
        }
        if (
          parseField(
            type,
            fields[field],
            searchIndexes,
            changedIndexes,
            types[type].fields,
            [field]
          )
        ) {
          changed = true
        }
      }
    }
    changedTypes.push(type)
  }

  await Promise.all(
    changedTypes.map(type => {
      return client.redis.hset('types', type, JSON.stringify(types[type]))
    })
  )

  if (changedIndexes.length) {
    await client.redis.hset(
      'schema',
      'searchIndexes',
      JSON.stringify(searchIndexes)
    )
    await Promise.all(
      changedIndexes.map(index => {
        return updateIndex(client.redis, index, searchIndexes[index])
      })
    )
  }
}

export default updateTypeSchema
