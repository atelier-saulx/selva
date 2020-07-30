import { SelvaClient } from '../..'
import { SetOptions } from '../types'
import { Schema, TypeSchema, FieldSchemaOther } from '../../schema'

function refs(field: string, payload: SetOptions, langs?: string[]): void {
  if (payload.$ref && Object.keys(payload).length !== 1) {
    throw new Error(`$ref only allow without other fields ${field} ${payload}`)
  }

  if (!langs) {
    return
  }

  for (const lang of langs) {
    if (payload[lang]) {
      if (payload[lang].$ref) {
        payload[lang] = `___selva_$ref:${payload[lang].$ref}`
      } else if (payload[lang].$default && payload[lang].$default.$ref) {
        payload[lang].$default = `___selva_$ref:${payload[lang].$default.$ref}`
      }
    }
  }
}

const verify = (
  payload: SetOptions,
  nested?: boolean,
  lang?: string[]
): void => {
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
      // TODO: handle this
    } else if (key === '$delete') {
      // $delete is allowed
      // TODO: use different type
    } else if (lang && lang.indexOf(key) !== -1) {
      if (typeof payload[key] === 'object') {
        verify(payload[key], true)
      } else if (typeof payload[key] !== 'string') {
        throw new Error(`Incorrect type for string ${key}`)
      }
    } else {
      throw new Error(`Incorrect field for text ${key} ${payload[key]}`)
    }
  }
}

export default async (
  _client: SelvaClient,
  schema: Schema,
  field: string,
  payload: SetOptions,
  result: SetOptions,
  _fields: FieldSchemaOther,
  _type: string,
  $lang?: string
): Promise<void> => {
  const lang: string[] = schema.languages

  if ($lang) {
    payload = { [$lang]: payload }
  }

  // refs(field, payload, lang)
  verify(payload, false, lang)

  const push = (o, hname: string) => {
    for (const k in o) {
      if (typeof o[k] === 'string') {
        result.push('0', `${hname}.${k}`, o[k])
      } else {
        push(o[k], `${hname}.${k}`)
      }
    }
  }

  push(payload, field)
}
