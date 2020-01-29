import { SearchIndexes, SearchSchema } from '~selva/schema/index'
import * as logger from '../logger'

function createIndex(index: string, schema: SearchSchema): void {
  const args = [index, 'SCHEMA']
  for (const field in schema) {
    args[args.length] = field
    for (const f of schema[field]) {
      args[args.length] = f
    }
  }

  const result = redis.pcall('ftCreate', ...args)
  if (result.err) {
    logger.error(`Error creating index ${index}: ${result.err}`)
  }
}

function alterIndex(index: string, schema: SearchSchema): void {
  for (const field in schema) {
    const result = redis.pcall(
      'ftAlter',
      index,
      'SCHEMA',
      'ADD',
      field,
      ...schema[field]
    )
    if (result.err) {
      logger.error(`Error altering index ${index}: ${result.err}`)
    }
  }
}

function updateIndex(index: string, schema: SearchSchema): void {
  const info: string[] = redis.pcall('ftInfo', index)
  if (!info || (<any>info).err) {
    logger.error(`Error fetch info for index ${index}: ${(<any>info).err}`)
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

  // TODO: for now we can remove this
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
