import { SetOptions } from './types'
import { Schema, TypeSchema } from '../schema'
import fieldParsers from './fieldParsers'
import { verifiers } from './fieldParsers/simple'

const ALLOWED_OPTIONS_DOCS = `
Record identification (if neither $id or $alias is provided, 'root' id is assumed)
- $id 
- $alias
General set operators
- $language: string (optional) (used to automatically fetch specified language from 'text' type fields and properties)
- $merge: boolean (optional) (whether set fields are merged to existing record fields, or set to override)
- $version: string (optional) TODO: version is not functional yet, coming soon
`

// TODO: add link to set payload by type
function allowedFieldsDoc(schemas: Schema, type?: string): string {
  let typeSchema: TypeSchema
  if (type) {
    typeSchema = schemas.types[type]
  }

  if (typeSchema) {
    let str = ''
    for (const key in typeSchema.fields) {
      str += `        - ${key}: ${typeSchema.fields[key].type} \n`
    }

    return str
  }

  return ''
}

export default function parseSetObject(
  payload: SetOptions,
  schemas: Schema,
  $lang?: string
): SetOptions {
  const result: SetOptions = { $args: [] }

  if (!payload.type && schemas.prefixToTypeMapping && payload.$id) {
    payload.type = schemas.prefixToTypeMapping[payload.$id.substring(0, 2)]
  }

  if (!payload.type && payload.$id === 'root') {
    payload.type = 'root'
  }

  if (payload.$language) {
    $lang = payload.$language
  }
  const type = payload.type

  const schema = type === 'root' ? schemas.rootType : schemas.types[type]
  if (!schema) {
    throw new Error(
      `Cannot find type ${type ||
        ` from prefix ${payload.$id.substring(
          0,
          2
        )}`} from set-object ${JSON.stringify(payload, null, 2)}`
    )
  }

  let fields = schema.fields
  for (let key in payload) {
    if (key[0] === '$') {
      if (key === '$merge') {
        if (!(payload[key] === true || payload[key] === false)) {
          throw new Error(`$merge needs to be a a boolean `)
        }
        result[key] = payload[key]
      } else if (key === '$id') {
        if (!verifiers.id(payload[key])) {
          throw new Error('Wrong type for $id ' + payload[key])
        }
        result[key] = payload[key]
      } else if (key === '$db') {
        if (typeof payload[key] !== 'string') {
          throw new Error('Wrong type for $db string expected: ' + payload[key])
        }
        result[key] = payload[key]
      } else if (key === '$operation') {
        const val = payload[key]
        if (val !== 'update' && val !== 'insert' && val !== 'upsert') {
          throw new Error('Wrong type for $operation ' + payload[key])
        }

        result[key] = payload[key]
      } else if (key === '$source') {
        if (
          typeof payload[key] !== 'string' &&
          typeof payload[key] !== 'object'
        ) {
          throw new Error('Wrong type for $source, string or object required')
        }

        result[key] = payload[key]
      } else if (key === '$alias') {
        if (typeof payload[key] !== 'string' && !Array.isArray(payload[key])) {
          throw new Error('Wrong type for $alias, string or array required')
        }

        result[key] = payload[key]
      } else if (key === '$_batchOpts') {
        // internally used
        result[key] = payload[key]
      } else if (key === '$version') {
        if (typeof payload[key] !== 'string') {
          throw new Error('Wrong type for $version, string required')
        }
        console.warn('$version is not implemented yet!')
        result[key] = payload[key]
      } else if (key === '$language') {
        if (
          typeof payload[key] !== 'string' ||
          String(payload[key]).length > 100
        ) {
          throw new Error(
            `Wrong type for language ${payload[key]}, string required`
          )
        }

        if (
          schemas &&
          schemas.languages &&
          !schemas.languages.includes(payload[key])
        ) {
          throw new Error(
            `Wrong value for language ${
              payload[key]
            }, schema allows the following languages to be set: ${schemas.languages.join(
              ', '
            )}`
          )
        }
      } else if (key === '$_itemCount') {
        // ignore
        result[key] = payload[key]
      } else {
        throw new Error(`Unsupported operator on set ${key}. Did you mean one following set operators?
          ${ALLOWED_OPTIONS_DOCS}`)
      }
    } else if (!fields[key]) {
      throw new Error(`
        Cannot find field ${key} in ${type}. Did you mean one of the following properties?
${allowedFieldsDoc(schemas, type)}
        `)
    } else {
      const fn = fieldParsers[fields[key].type]
      fn(schemas, key, payload[key], result, fields[key], type, $lang)
    }
  }
  return result
}
