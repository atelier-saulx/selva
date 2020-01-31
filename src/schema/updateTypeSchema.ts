import { SelvaClient } from '..'
import {
  Types,
  SearchIndexes,
  FieldSchema,
  Search,
  TypeSchema,
  HierarchySchema,
  defaultFields
} from './'
import updateIndex from './updateIndex'

const searchChanged = (newSearch: Search, oldSearch: Search): boolean => {
  if (newSearch.index !== oldSearch.index) {
    return true
  }
  if (newSearch.type.length !== oldSearch.type.length) {
    return true
  }
  for (let i = 0; i < newSearch.type.length; i++) {
    if (newSearch.type[i] !== oldSearch.type[i]) {
      return true
    }
  }
  return false
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
      console.log(type, path, field, searchIndexes)
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

const isEqual = (a: any, b: any): boolean => {
  const type = typeof a
  if (type !== typeof b) {
    return false
  }
  if (type === 'object') {
    for (let key in a) {
      if (!b[key]) {
        return false
      } else {
        if (!isEqual(a[key], b[key])) {
          return false
        }
      }
    }
  } else if (a !== b) {
    return false
  }
  return true
}

const luaUpdateAncestorsForType = async (client: SelvaClient, type: string) => {
  console.log('  🥶 SEND UPDATE ANCESTORS FOR TYPE!', type)
}

// needs to potentially re-index everything
const updateHierarchy = async (
  client: SelvaClient,
  prev: TypeSchema,
  newHierarchy: HierarchySchema
): Promise<boolean> => {
  let changed: boolean = false
  if (!isEqual(prev.hierarchy, newHierarchy)) {
    prev.hierarchy = newHierarchy
    changed = true
  }
  return changed
}

async function updateTypeSchema(
  client: SelvaClient,
  props: Types,
  types: Types,
  searchIndexes: SearchIndexes
): Promise<void> {
  const changedTypes: string[] = []
  const changedIndexes: Set<string> = new Set()
  const changedTypesHierachies: string[] = []

  for (const type in props) {
    let changed: boolean = false
    let newType: boolean = false
    if (!types[type]) {
      types[type] = {}
      newType = true
    }

    if (props[type].hierarchy || props[type].hierarchy === false) {
      if (await updateHierarchy(client, types[type], props[type].hierarchy)) {
        changed = true
        if (!newType) changedTypesHierachies.push(type)
      }
    }

    if (props[type].prefix) {
      // this is just fine (allrdy checked in id)
      types[type].prefix = props[type].prefix
    }

    let fields = props[type].fields

    if (newType) {
      if (!fields) {
        fields = {}
      }
      props[type].fields = fields = { ...defaultFields, ...fields }
    }

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
      return client.redis.hset(
        '___selva_types',
        type,
        JSON.stringify(types[type])
      )
    })
  )

  if (changedIndexes.size) {
    await client.redis.hset(
      '___selva_schema',
      'searchIndexes',
      JSON.stringify(searchIndexes)
    )
    await Promise.all(
      Array.from(changedIndexes).map(index => {
        return updateIndex(client.redis, index, searchIndexes[index])
      })
    )
  }

  await Promise.all(
    changedTypesHierachies.map(type => {
      return luaUpdateAncestorsForType(client, type)
    })
  )
}

export default updateTypeSchema
