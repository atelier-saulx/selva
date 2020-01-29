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

  const result = redis.pcall('ft.create', ...args)
  if (result.err) {
    logger.error(`Error creating index ${index}: ${result.err}`)
  }
}

function alterIndex(index: string, schema: SearchSchema): void {
  for (const field in schema) {
    const result = redis.pcall(
      'ft.alter',
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

function findFieldsFromInfoReply(info: string[]): string | null {
  for (let i = 0; i < info.length; i++) {
    if (info[i] === 'fields') {
      return info[i + 1]
    }
  }

  return null
}

function updateIndex(index: string, schema: SearchSchema): void {
  const info: string[] = redis.pcall('ft.info', index)
  if (!info || (<any>info).err) {
    logger.error(`Error fetch info for index ${index}: ${(<any>info).err}`)
    return createIndex(index, schema)
  }
  const fields = findFieldsFromInfoReply(info)

  if (!fields) {
    return createIndex(index, schema)
  }

  // FIXME: if super different (e.g. fields differently indexed) then drop the index
  return alterIndex(index, schema)
}

export default function updateSearchIndexes(
  changedSearchIndexes: Record<string, boolean>,
  indexes: SearchIndexes
): void {
  logger.info(`Updating search indexes ${cjson.encode(changedSearchIndexes)}`)
  for (const index in changedSearchIndexes) {
    updateIndex(index, indexes[index])
  }
}
