import { getSearchIndexes, getSchema } from '../schema/index'
import * as logger from '../logger'
import { splitString, hasExistsIndex } from '../util'
import { SetOptions } from '~selva/set/types'
import { getTypeFromId } from 'lua/src/typeIdMapping'

const mapLanguages = (lang: string): string => {
  lang = splitString(lang, '_')[0]
  if (lang === 'en') {
    return 'english'
  } else if (lang === 'de') {
    return 'german'
  } else if (lang === 'da') {
    return 'danish'
  } else if (lang === 'it') {
    return 'italian'
  } else if (lang === 'fr') {
    return 'french'
  } else if (lang === 'nl') {
    return 'dutch'
  } else if (lang === 'no') {
    return 'norwegian'
  } else if (lang === 'ru') {
    return 'russian'
  } else if (lang === 'hu') {
    return 'hungarian'
  } else if (lang === 'pt') {
    return 'portuguese'
  } else if (lang === 'ar') {
    return 'arabic'
  } else if (lang === 'ron') {
    return 'romanian'
  } else if (lang === 'fi') {
    return 'finnish'
  } else if (lang === 'es') {
    return 'spanish'
  } else if (lang === 'se') {
    return 'swedish'
  } else if (lang === 'ta') {
    return 'tamil'
  } else if (lang === 'tr') {
    return 'turkish'
  } else if (lang === 'zh') {
    return 'chinese'
  } else {
    return 'en'
  }
}

const getDotIndex = (str: string): number => {
  for (let i = str.length - 1; i > 1; i--) {
    if (str[i] === '.') {
      return i
    }
  }
  return -1
}

export function addFieldToSearch(
  id: string,
  field: string,
  value: string
): void {
  const searchIndex = getSearchIndexes()

  for (const indexKey in searchIndex) {
    const index = searchIndex[indexKey]
    if (index[field]) {
      redis.pcall(
        'ft.add',
        indexKey,
        id,
        '1',
        'NOSAVE',
        'REPLACE',
        'PARTIAL',
        'FIELDS',
        field,
        value
      )

      if (hasExistsIndex(index[field])) {
        logger.info('INDEXING EXISTS', id, field)
        redis.call('hset', id, '_exists_' + field, 'T')
        redis.pcall(
          'ft.add',
          indexKey,
          id,
          '1',
          'NOSAVE',
          'REPLACE',
          'PARTIAL',
          'FIELDS',
          '_exists_' + field,
          'T'
        )
      }
    } else {
      const lastDotIndex = getDotIndex(field)
      if (lastDotIndex) {
        const fieldToCheck = field.substring(0, lastDotIndex)
        if (index[fieldToCheck]) {
          if (index[fieldToCheck][0] === 'TEXT-LANGUAGE') {
            const lang = field.substring(lastDotIndex + 1)
            const mapped = mapLanguages(lang)
            redis.pcall(
              'ft.add',
              indexKey,
              id,
              '1',
              'LANGUAGE',
              mapped,
              'NOSAVE',
              'REPLACE',
              'PARTIAL',
              'FIELDS',
              field,
              value
            )

            if (hasExistsIndex(index[field])) {
              logger.info('INDEXING EXISTS', id, field)
              redis.call('hset', id, '_exists_' + field, 'T')
              redis.pcall(
                'ft.add',
                indexKey,
                id,
                '1',
                'NOSAVE',
                'REPLACE',
                'PARTIAL',
                'FIELDS',
                '_exists_' + field,
                'T'
              )
            }
          }
        }
      }
    }
  }
}

export function indexMissingWithExists(
  payload: WithRequired<SetOptions, 'id'>
) {
  const schema = getSchema()

  logger.info('PAYLOAD', payload)
  const typeSchema =
    payload.$id === 'root'
      ? schema.rootType
      : schema.types[getTypeFromId(payload.$id)]

  for (const fieldName in typeSchema.fields) {
    const field = typeSchema.fields[fieldName]
    if (field.type !== 'object' && field.search) {
      const search = field.search
      if (search !== true) {
        if (hasExistsIndex(search) && !payload[fieldName]) {
          logger.info('INDEXING EXISTS', fieldName)
          const index = search.index || 'default'
          redis.pcall(
            'ft.add',
            index,
            payload.$id,
            '1',
            'NOSAVE',
            'REPLACE',
            'PARTIAL',
            'FIELDS',
            '_exists_' + fieldName,
            'F'
          )
        }
      }
    }
  }
}
