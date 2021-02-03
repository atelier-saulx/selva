import { SearchIndexes, SearchSchema, Schema } from '~selva/schema/index'
import { hasExistsIndex } from '../util'
import * as logger from '../logger'
import { isTextIndex } from '../util'

function createIndex(
  index: string,
  schema: SearchSchema,
  languages: string[]
): void {
  const args = [index, 'NOHL', 'NOFREQS', 'SCHEMA']

  for (const field in schema) {
    const value = schema[field]
    if (isTextIndex(value)) {
      for (let i = 0; i < languages.length; i++) {
        args[args.length] = '___escaped:' + field + '.' + languages[i]
        args[args.length] = 'TEXT'
        for (let i = 1; i < value.length; i++) {
          if (value[i] !== 'EXISTS') {
            args[args.length] = value[i]
          }
        }
      }
    } else if (value[0] !== 'EXISTS') {
      args[args.length] = field
      for (const f of schema[field]) {
        if (f !== 'EXISTS') {
          args[args.length] = f
        }
      }
    }

    if (hasExistsIndex(value)) {
      args[args.length] = '_exists_' + field
      args[args.length] = 'TAG'
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
    if (isTextIndex(schema[field])) {
      const args = schema[field][1] ? ['TEXT', schema[field][1]] : ['TEXT']
      for (let i = 0; i < languages.length; i++) {
        const langField = '___escaped:' + field + '.' + languages[i]
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
    } else if (schema[field][0] !== 'EXISTS') {
      const indexArgs: string[] = []
      for (const a of schema[field]) {
        if (a !== 'EXISTS') {
          indexArgs[indexArgs.length] = a
        }
      }

      const result = redis.pcall(
        'ft.alter',
        index,
        'SCHEMA',
        'ADD',
        field,
        ...indexArgs
      )

      if (result.err && result.err !== 'Duplicate field in schema') {
        logger.error(`Error altering index ${index} ${field}: ${result.err}`)
      }
    }

    if (hasExistsIndex(schema[field])) {
      const result = redis.pcall(
        'ft.alter',
        index,
        'SCHEMA',
        'ADD',
        '_exists_' + field,
        'TAG'
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
  // for (const index in changedSearchIndexes) {
  //   updateIndex(index, indexes[index], languages)
  // }
}
