import { SetOptions } from './types'
import { Schema, TypeSchema } from '../schema'
import { SelvaClient } from '..'
import fieldParsers from './fieldParsers'
import * as verifiers from '@saulx/validators'
import { id2type } from '../util'

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

export default async function parseSetObject(
  client: SelvaClient,
  payload: SetOptions,
  schemas: Schema,
  $lang?: string,
  noTarget?: boolean
): Promise<string[]> {
  // id, R|N, field enum, fieldName, value
  const result: string[] = ['']

  if (noTarget) {
    // dont do too many things special
  } else {
    if (!payload.$id && !payload.$alias) {
      payload.$id = await client.id({
        db: payload.$db || 'default',
        type: payload.type,
      })
    }

    if (payload.$id) {
      ;(<any>result).$id = payload.$id
    }
  }

  if (payload.$db) {
    ;(<any>result).$db = payload.$db
  }

  // for batched sets
  ;(<any>result).$extraQueries = []

  //  && (<any>payload.parents).$noRoot
  if (payload.parents) {
    result[0] += 'N'
  }

  if (!payload.type && schemas.prefixToTypeMapping && payload.$id) {
    payload.type = schemas.prefixToTypeMapping[id2type(payload.$id)]
  }

  if (!payload.type && payload.$id === 'root') {
    payload.type = 'root'
  }
  ;(<any>result).$type = payload.type

  if (payload.$language) {
    $lang = payload.$language
  }
  const type = payload.type

  const schema = type === 'root' ? schemas.rootType : schemas.types[type]
  if (!schema) {
    throw new Error(
      `Cannot find type ${
        type || `from prefix ${id2type(payload.$id)}`
      } from set-object ${JSON.stringify(payload, null, 2)}`
    )
  }

  const fields = schema.fields

  for (const key in payload) {
    if (key === 'type') {
      // Drop
    } else if (key[0] === '$') {
      if (key === '$merge') {
        if (!(payload[key] === true || payload[key] === false)) {
          throw new Error(`$merge needs to be a a boolean `)
        }
        if (payload[key] === false) {
          result[0] += 'M'
        }
      } else if (key === '$id') {
        if (!verifiers.id(payload[key])) {
          throw new Error('Wrong type for $id ' + payload[key])
        }
      } else if (key === '$db') {
        if (typeof payload[key] !== 'string') {
          throw new Error('Wrong type for $db string expected: ' + payload[key])
        }
      } else if (key === '$operation') {
        const val = payload[key]
        if (val !== 'update' && val !== 'insert' && val !== 'upsert') {
          throw new Error('Wrong type for $operation ' + payload[key])
        }

        if (val === 'insert') {
          result[0] += 'C'
        } else if (val === 'update') {
          result[0] += 'U'
        }
      } else if (key === '$source') {
        if (
          typeof payload[key] !== 'string' &&
          typeof payload[key] !== 'object'
        ) {
          throw new Error('Wrong type for $source, string or object required')
        }

        // TODO: $source, pass this as struct field
        // result.push('5', '$source', payload.$source)
      } else if (key === '$alias') {
        const aliasIsArray = Array.isArray(payload[key])

        if (typeof payload[key] !== 'string' && !aliasIsArray) {
          throw new Error('Wrong type for $alias, string or array required')
        }

        const arr = aliasIsArray ? payload[key] : [payload[key]]
        result.push('6', key, arr.map((s) => `${s}\0`).join(''))
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

        // support languages
        ;(<any>result).$language = payload.$language
      } else {
        throw new Error(`Unsupported operator on set ${key}. Did you mean one following set operators?
          ${ALLOWED_OPTIONS_DOCS}`)
      }
    } else if (!fields[key]) {
      // make this a bit nicer

      throw new Error(`
        Cannot find field ${key} in ${type}. Did you mean one of the following properties?
${allowedFieldsDoc(schemas, type)}
        `)
    } else {
      const fn = fieldParsers[fields[key].type]
      await fn(
        client,
        schemas,
        key,
        payload[key],
        result,
        fields[key],
        type,
        $lang
      )
    }
  }

  // Only send type field if there are no other changes
  if (result.length <= 1) {
    result.push('0', 'type', payload.type)
  }
  return result
}
