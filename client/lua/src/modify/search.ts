import { getSearchIndexes, getSchema } from '../schema/index'
import * as logger from '../logger'
import { SetOptions } from '~selva/set/types'
import { getTypeFromId } from 'lua/src/typeIdMapping'
import {
  splitString,
  isTextIndex,
  joinString,
  escapeSpecial,
  hasExistsIndex,
  stringStartsWith
} from '../util'

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

export function hasSearch(id: string, field: string): boolean {
  const searchIndex = getSearchIndexes()

  for (const indexKey in searchIndex) {
    const index = searchIndex[indexKey]
    if (index[field]) {
      return true
    }
  }

  return false
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
      const v = redis.pcall(
        'ft.add',
        indexKey,
        id,
        '1',
        'NOSAVE',
        'REPLACE',
        'PARTIAL',
        'FIELDS',
        field,
        tostring(value)
      )

      if (hasExistsIndex(index[field])) {
        redis.call('hset', id, '_exists_' + field, 'T')
        const v = redis.pcall(
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
          const escaped = escapeSpecial(value)
          const lang = field.substring(lastDotIndex + 1)

          if (isTextIndex(index[fieldToCheck])) {
            const escapedFieldName = '___escaped:' + field
            const allLanguages = getSchema().languages || []

            redis.call('hset', id, escapedFieldName, escaped)

            const mapped = mapLanguages(lang)
            const v = redis.pcall(
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
              escapedFieldName,
              escaped
            )

            const allKeys: string[] = redis.call('hkeys', id)
            // FIXME: this can be cached per script execution later
            const allSetLanguages: Record<string, true> = {}
            for (const key of allKeys) {
              if (stringStartsWith(key, fieldToCheck)) {
                const lastDotIdx = getDotIndex(key)
                const l = key.substring(lastDotIdx + 1, key.length)
                allSetLanguages[l] = true
              }
            }

            let hasPrecedence = false
            let hasSecondaryPrecedence = true

            // initialize hasSecondaryPrecedence
            for (let i = 0; i < allLanguages.length; i++) {
              const otherLang = allLanguages[i]
              if (otherLang === lang) {
                break
              } else if (allSetLanguages[otherLang]) {
                hasSecondaryPrecedence = false
                break
              }
            }

            for (const otherLang of allLanguages) {
              const exists = allSetLanguages[otherLang]
              const escapedFieldName =
                '___escaped:' + fieldToCheck + '.' + otherLang

              if (exists) {
                hasSecondaryPrecedence = false
              }

              if (otherLang === lang) {
                // do nothing
                hasPrecedence = true
              } else {
                let replaceValue = false
                if (!exists) {
                  replaceValue = hasPrecedence || hasSecondaryPrecedence
                }

                if (replaceValue) {
                  redis.call('hset', id, escapedFieldName, escaped)

                  redis.pcall(
                    'ft.add',
                    indexKey,
                    id,
                    '1',
                    'LANGUAGE',
                    mapLanguages(otherLang),
                    'NOSAVE',
                    'REPLACE',
                    'PARTIAL',
                    'FIELDS',
                    escapedFieldName,
                    escaped
                  )
                } else {
                  const didSet = redis.call(
                    'hsetnx',
                    id,
                    escapedFieldName,
                    escaped
                  )

                  if (didSet === 1) {
                    redis.pcall(
                      'ft.add',
                      indexKey,
                      id,
                      '1',
                      'LANGUAGE',
                      mapLanguages(otherLang),
                      'NOSAVE',
                      'REPLACE',
                      'PARTIAL',
                      'FIELDS',
                      escapedFieldName,
                      escaped
                    )
                  }
                }
              }
            }

            if (hasExistsIndex(index[field])) {
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

            if (index[fieldToCheck][0] === 'TEXT-LANGUAGE-SUG') {
              // if suggestion, also add to dictionary
              const words = splitString(escaped, ' ')
              for (let i = words.length - 2; i >= 0; i--) {
                let searchTerms: string = ''
                for (let j = i; j < words.length; j++) {
                  searchTerms += words[j] + ' '
                }

                const str = searchTerms.substr(0, searchTerms.length - 1)
                addSuggestion(str, lang)
              }

              for (const word of words) {
                addSuggestion(word, lang)
              }
            }
          }
        }
      }
    }
  }
}

function addSuggestion(sug: string, _lang: string) {
  const current: number = redis.call('hincrby', `sug_counts`, sug, '1')
  if (current === 1) {
    logger.info('ft.sugadd', `sug`, sug, '1')
    const v = redis.pcall('ft.sugadd', `sug`, sug, '1')
  } else {
    logger.info(
      `ft.sugadd -- exists, incrementing to ${current}`,
      `sug`,
      sug,
      '1'
    )
  }
}
