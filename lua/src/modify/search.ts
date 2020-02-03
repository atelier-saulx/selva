import { getSchema, getSearchIndexes } from '../schema/index'
import * as logger from '../logger'

const mapLanguages = (lang: string) => {
  /*
    supported languages
  "romanian", "russian", "spanish", "swedish", "tamil", "turkish" "chinese"
    */

  if (lang === 'en') {
    return 'english'
  } else if (lang === 'de') {
    return 'german'
  } else if (lang === 'da') {
    return 'danish'
  } else if (lang === 'it') {
    return 'italian'
  } else if (lang === 'fi') {
    return 'finnish'
  } else if (lang === 'fr') {
    return 'french'
  } else if (lang === 'nl') {
    return 'dutch'
  } else if (lang === 'no') {
    return 'norwegian'
  } else if (lang === 'hu') {
    return 'hungarian'
  } else if (lang === 'pt') {
    return 'portuguese'
  } else if (lang === 'ar') {
    return 'arabic'
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
      redis.call(
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
          if (index[fieldToCheck][0] === 'TEXT-LANGUAGE') {
            const lang = field.substring(lastDotIndex + 1)
            logger.info(`index for language!! ${lang}`)
          }
        }
      }
    }
  }
}
