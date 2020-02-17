import { getSearchIndexes } from '../schema/index'
import * as logger from '../logger'
import { splitString, isTextIndex, joinString } from '../util'

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
    } else {
      const lastDotIndex = getDotIndex(field)
      if (lastDotIndex) {
        const fieldToCheck = field.substring(0, lastDotIndex)
        if (index[fieldToCheck]) {
          const lang = field.substring(lastDotIndex + 1)

          if (isTextIndex(index[fieldToCheck])) {
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
          }

          if (index[fieldToCheck][0] === 'TEXT-LANGUAGE-SUG') {
            // if suggestion, also add to dictionary
            const words = splitString('value', ' ')
            for (let i = words.length - 1; i >= 0; i--) {
              const searchTerms: string[] = []
              for (let j = 0; j <= i; j++) {
                searchTerms[i] = words[i]
              }

              redis.pcall(
                'ft.sugadd',
                `sug_${lang}`,
                joinString(searchTerms, ' '),
                '1'
              )
            }
          }
        }
      }
    }
  }
}
