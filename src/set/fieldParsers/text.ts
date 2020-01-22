import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaOther } from '../../schema'

const verify = (payload: SetOptions, nested?: boolean, lang?: string[]) => {
  for (let key in payload) {
    if (key === '$merge') {
      if (nested) {
        throw new Error(`$merge cannot be used on language fields`)
      }
      if (!(payload[key] === false || payload[key] === true)) {
        throw new Error(`$merge needs to be true or false ${key}`)
      }
    } else if (key === '$value' || key === '$default') {
      if (nested) {
        if (typeof payload[key] !== 'string') {
          throw new Error(`Incorrect type for string ${key}`)
        }
      } else {
        verify(payload[key], false, lang)
      }
    } else if (lang && lang.indexOf(key) !== -1) {
      if (typeof payload[key] === 'object') {
        verify(payload[key], true)
      } else if (typeof payload[key] !== 'string') {
        throw new Error(`Incorrect type for string ${key}`)
      }
    } else {
      throw new Error(`Incorrect field for text ${key} ${payload}`)
    }
  }
}

export default (
  schemas: Record<string, TypeSchema>,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  fields: FieldSchemaOther,
  type: string
): void => {
  const lang: string[] = <string[]>schemas._languages
  verify(payload, false, lang)
  result[field] = payload
}
