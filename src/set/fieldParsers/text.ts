import { SetOptions } from '../types'
import { TypeSchema, FieldSchemaOther } from '../../schema'

function refs(field: string, payload: SetOptions, langs?: string[]): void {
  if (payload.$ref && Object.keys(payload).length !== 1) {
    throw new Error(`$ref only allow without other fields ${field} ${payload}`)
  }

  if (!langs) {
    return
  }

  for (const lang of langs) {
    if (payload[lang] && payload[lang].$ref) {
      payload[lang] = `___selva_$ref:${payload[lang].$ref}`
    }
  }
}

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
    } else if (key === '$ref') {
      // $refs are allowed
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
  refs(field, payload, lang)
  verify(payload, false, lang)
  result[field] = payload
}
