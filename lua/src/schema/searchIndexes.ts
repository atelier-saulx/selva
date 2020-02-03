import { SearchIndexes, SearchSchema, Schema } from '~selva/schema/index'
import * as logger from '../logger'

function createIndex(
  index: string,
  schema: SearchSchema,
  languages: string[]
): void {
  const args = [index, 'NOHL', 'NOFREQS', 'SCHEMA']

  for (const field in schema) {
    const value = schema[field]
    if (value[0] === 'TEXT-LANGUAGE') {
      for (let i = 0; i < languages.length; i++) {
        args[args.length] = field + '.' + languages[i]
        args[args.length] = 'TEXT'
        for (let i = 1; i < value.length; i++) {
          args[args.length] = value[i]
        }
      }
    } else {
      args[args.length] = field
      for (const f of schema[field]) {
        args[args.length] = f
      }
    }
  }

  const result = redis.pcall('ft.create', ...args)
  if (result.err) {
    logger.error(`Error creating index ${index}: ${result.err}`)
  }
}

function alterIndex(
  index: string,
  schema: SearchSchema,
  languages: string[]
): void {
  for (const field in schema) {
    if (schema[field][0] === 'TEXT-LANGUAGE') {
      const args = schema[field][1] ? ['TEXT', schema[field][1]] : ['TEXT']
      for (let i = 0; i < languages.length; i++) {
        const langField = field + '.' + languages[i]
        const result = redis.pcall(
          'ft.alter',
          index,
          'SCHEMA',
          'ADD',
          langField,
          ...args
        )
        if (result.err && result.err !== 'Duplicate field in schema') {
          logger.error(
            `Error altering index for language ${index} ${langField}: ${result.err}`
          )
        }
      }
    } else {
      const result = redis.pcall(
        'ft.alter',
        index,
        'SCHEMA',
        'ADD',
        field,
        ...schema[field]
      )

      if (result.err && result.err !== 'Duplicate field in schema') {
        logger.error(`Error altering index ${index} ${field}: ${result.err}`)
      }
    }
  }
}

function findFieldsFromInfoReply(
  info: ({ ok: string; err: string } | { ok: string }[])[]
): string | null {
  for (let i = 0; i < info.length; i++) {
    // @ts-ignore
    if (info[i].ok === 'fields') {
      // @ts-ignore
      return info[i + 1]
    }
  }

  return null
}

function updateIndex(
  index: string,
  schema: SearchSchema,
  languages: string[]
): void {
  const info: { ok: string; err: string }[] = redis.pcall('ft.info', index)
  if (!info || (<any>info).err) {
    if ((<any>info).err !== 'Unknown Index name') {
      logger.error(`Error fetch info for index ${index}: ${(<any>info).err}`)
    }
    return createIndex(index, schema, languages)
  }

  const fields = findFieldsFromInfoReply(info)

  if (!fields) {
    return createIndex(index, schema, languages)
  }

  // FIXME: if super different (e.g. fields differently indexed) then drop the index
  return alterIndex(index, schema, languages)
}

export default function updateSearchIndexes(
  changedSearchIndexes: Record<string, boolean>,
  indexes: SearchIndexes,
  schema: Schema
): void {
  const languages = schema.languages || ['en']
  logger.info(
    `Updating search indexes ${cjson.encode(
      changedSearchIndexes
    )} ${cjson.encode(languages)}`
  )
  for (const index in changedSearchIndexes) {
    updateIndex(index, indexes[index], languages)
  }
}
