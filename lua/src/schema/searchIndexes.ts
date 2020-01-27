import { SearchIndexes, SearchSchema } from '~selva/schema/index'

function updateIndex(index: string, schema: SearchSchema): void {
  const info: string[] = redis.call('ftInfo', index)
  if (!info) {
    return createIndex(index, schema)
  }
  const fields = info[info.indexOf('fields') + 1]

  if (!fields) {
    return createIndex(index, schema)
  }

  const set: Record<string, boolean> = {}
  let changed: boolean = false
  for (const field of fields) {
    const scheme = schema[field]
    set[field] = true
    if (scheme) {
      for (let i = 0; i < scheme.length; i++) {
        if (scheme[i] !== type[i]) {
          changed = true
          break
        }
      }

      if (changed) {
        break
      }
    }
  }

  if (!changed) {
    for (const field in schema) {
      if (!set[field]) {
        changed = true
        break
      }
    }
  }
  if (changed) {
    // if super different (e.g. fields differently indexed) then drop the index
    return alterIndex(index, schema)
  }
}

export default function updateSearchIndexes(
  changedSearchIndexes: Record<string, boolean>,
  indexes: SearchIndexes
): void {
  for (const index in changedSearchIndexes) {
    updateIndex(index, indexes[index])
  }
}
