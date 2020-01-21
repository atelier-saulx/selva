import { SelvaClient } from '..'
import { Types, SearchIndexes, FieldSchema, Search } from './'
import updateIndex from './updateIndex'

const searchChanged = (newSearch: Search, oldSearch: Search): boolean => {
  console.log('check difference', newSearch, oldSearch)
  return true
}

const parseField = (
  type: string,
  field: FieldSchema,
  searchIndexes: SearchIndexes,
  changedIndexes: Set<string>,
  props: FieldSchema | { [key: string]: FieldSchema },
  path: string[]
  // previous
): boolean => {
  let changed = false
  let segment = props
  for (let i = 0; i < path.length; i++) {
    const key = path[i]
    if (i === path.length - 1) {
      const prev = segment[key]
      if (prev) {
        if (field.type !== prev.type) {
          throw new Error(
            `Cannot change existing type for ${type} field ${path} changing from ${prev.type} to ${field.type}`
          )
        }
      } else {
        changed = true
        segment[key] = {
          type: field.type
        }
      }
      segment = segment[key]
    } else {
      segment = segment[key] || (segment[key] = {})
    }
  }

  if (
    field.type !== 'object' &&
    field.type !== 'set' &&
    segment.type === field.type // weird typescript check
  ) {
    if (field.search) {
      const index = field.search.index || 'default'
      if (!segment.search || searchChanged(field.search, segment.search)) {
        if (!searchIndexes[index]) {
          searchIndexes[index] = {}
        }
        searchIndexes[index][
          path
            .filter(v => {
              return v !== 'properties' && v !== 'items'
            })
            .join('.')
        ] = field.search.type
        changedIndexes.add(index)
      }
      segment.search = field.search
    }
  }

  if (field.type === 'object' || field.type === 'json') {
    if (field.properties) {
      for (let key in field.properties) {
        if (
          parseField(
            type,
            field.properties[key],
            searchIndexes,
            changedIndexes,
            props,
            [...path, 'properties', key]
          )
        ) {
          changed = true
        }
      }
    }
  } else if (field.type === 'set' || field.type === 'array') {
    if (
      parseField(type, field.items, searchIndexes, changedIndexes, props, [
        ...path,
        'items'
      ])
    ) {
      changed = true
    }
  }

  // if object or json or array or set
  return changed
}

// needs to potentially re-index everything
const updateHierarchy = async () => {}

async function updateTypeSchema(
  client: SelvaClient,
  props: Types,
  types: Types,
  searchIndexes: SearchIndexes
): Promise<void> {
  const changedTypes: string[] = []
  const changedIndexes: Set<string> = new Set()

  for (const type in props) {
    let changed: boolean = false
    if (!types[type]) {
      types[type] = {}
    }

    if (props[type].hierarchy) {
      // parse it!
      types[type].hierarchy = props[type].hierarchy
    }

    if (props[type].prefix) {
      // this is just fine (allrdy checked in id)
      types[type].prefix = props[type].prefix
    }

    const fields = props[type].fields
    if (fields) {
      if (!types[type].fields) {
        types[type].fields = {}
      }

      for (let field in fields) {
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
    if (changed) {
      changedTypes.push(type)
    }
  }

  await Promise.all(
    changedTypes.map(type => {
      return client.redis.hset('types', type, JSON.stringify(types[type]))
    })
  )

  if (changedIndexes.size) {
    await client.redis.hset(
      'schema',
      'searchIndexes',
      JSON.stringify(searchIndexes)
    )
    await Promise.all(
      Array.from(changedIndexes).map(index => {
        return updateIndex(client.redis, index, searchIndexes[index])
      })
    )
  }
}

export default updateTypeSchema
