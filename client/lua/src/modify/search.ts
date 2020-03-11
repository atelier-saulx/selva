import { getSearchIndexes, getSchema } from '../schema/index'
import * as logger from '../logger'
import { SetOptions } from '~selva/set/types'
import { getTypeFromId } from 'lua/src/typeIdMapping'
import {
  splitString,
  isTextIndex,
  joinString,
  escapeSpecial,
  hasExistsIndex
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

      logger.info('PCALL', cjson.encode(v))
      console.log('PCALL', cjson.encode(v))

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

        logger.info('PCALL', v)
        console.log('PCALL', cjson.encode(v))
      }
    } else {
      const lastDotIndex = getDotIndex(field)
      if (lastDotIndex) {
        const fieldToCheck = field.substring(0, lastDotIndex)
        if (index[fieldToCheck]) {
          const escaped = escapeSpecial(value)
          const lang = field.substring(lastDotIndex + 1)

          if (isTextIndex(index[fieldToCheck])) {
            redis.call('hset', id, '___escaped:' + field, escaped)

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
              '___escaped:' + field,
              escaped
            )

            logger.info('PCALL', v)
            console.log('PCALL', cjson.encode(v))

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

function addSuggestion(sug: string, lang: string) {
  const current: number = redis.call('hincrby', `sug_${lang}_counts`, sug, '1')
  if (current === 1) {
    logger.info('ft.sugadd', `sug_${lang}`, sug, '1')
    const v = redis.pcall('ft.sugadd', `sug_${lang}`, sug, '1')
    logger.info('PCALL', v)
    console.log('PCALL', cjson.encode(v))
  } else {
    logger.info(
      `ft.sugadd -- exists, incrementing to ${current}`,
      `sug_${lang}`,
      sug,
      '1'
    )
  }
}
