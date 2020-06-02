import { GetOptions } from '../types'
import { SelvaClient } from '../..'

import checkAllowed from './checkAllowed'

import validateField from './field'
import validateInherit from './inherit'
import validateList from './list'
import validateFind from './find'

export type ExtraQuery = {
  getOpts: GetOptions
  path: string
  placeholder: GetOptions | true
  type: 'reference' | 'references' | 'nested_query'
}
export type ExtraQueries = Record<string, ExtraQuery[]>

export function addExtraQuery(extraQueries: ExtraQueries, query: ExtraQuery) {
  const db = query.getOpts.$db
  const current = extraQueries[db] || []
  current.push(query)
  extraQueries[db] = current
}

function transformDb(
  extraQueries: ExtraQueries,
  _client: SelvaClient,
  props: GetOptions,
  path: string
): void {
  if (props.$id || props.$alias) {
    addExtraQuery(extraQueries, {
      getOpts: props,
      path,
      placeholder: { $value: {} },
      type: 'nested_query'
    })
  } else {
    let val: GetOptions | true = true

    if (props.$field) {
      val = { $field: props.$field }
    } else if (
      props.$list &&
      typeof props.$list === 'object' &&
      props.$list.$find
    ) {
      console.log('TRANSFORMING LIST FIND', props)
      if (props.$list.$find.$traverse) {
        val = { $field: props.$list.$find.$traverse }
      }
    } else if (props.$find) {
      console.log('TRANSFORMING FIND', props)
      if (props.$find.$traverse) {
        val = { $field: props.$find.$traverse }
      }
    }

    addExtraQuery(extraQueries, {
      getOpts: props,
      path,
      placeholder: val,
      type: props.$list || props.$find ? 'references' : 'reference'
    })
  }
}

function validateNested(
  extraQueries: ExtraQueries,
  client: SelvaClient,
  props: GetOptions | true,
  path: string
): void {
  if (props === true) {
    return
  }

  if (props.$db) {
    return transformDb(extraQueries, client, props, path)
  }

  if (props.$id || props.$alias) {
    return validateTopLevel(extraQueries, client, props, path)
  }

  if (Array.isArray(props)) {
    for (let i = 0; i < props.length; i++) {
      validateNested(extraQueries, client, props[i], path + '.' + String(i))
    }

    return
  }

  const typeOf = typeof props
  if (typeOf !== 'object' && typeOf !== 'boolean') {
    throw new Error(
      `Field ${path} should be a boolean or an object, got ${props}`
    )
  }

  for (const field in props) {
    if (field.startsWith('$')) {
      if (field === '$field') {
        validateField(extraQueries, client, props.$field, path)
      } else if (field === '$inherit') {
        validateInherit(client, props.$inherit, path)
      } else if (field === '$list') {
        validateList(props, client, props.$list, path)
      } else if (field === '$find') {
        validateFind(props, client, props.$find, path)
      } else if (field === '$default') {
        continue
      } else if (field === '$all') {
        if (typeof props.$all !== 'boolean') {
          throw new Error(
            `Operator $all for ${path}.$all must be a boolean, got ${props.$all}`
          )
        }
      } else if (field === '$value') {
        const allowed = checkAllowed(props, new Set(['$value']))
        if (allowed !== true) {
          throw new Error(
            `Operator $value should not exist with any other operators, ${allowed} found`
          )
        }
      } else {
        throw new Error(
          `Operator ${field} is not supported in nested fields for ${path +
            '.' +
            field}`
        )
      }
    }
  }

  for (const field in props) {
    if (!field.startsWith('$')) {
      validateNested(extraQueries, client, props[field], path + '.' + field)
    }
  }
}

export default function validateTopLevel(
  extraQueries: ExtraQueries,
  client: SelvaClient,
  props: GetOptions,
  path: string = ''
): void {
  for (const field in props) {
    if (field.startsWith('$')) {
      if (field === '$db') {
        if (typeof props.$db !== 'string') {
          throw new Error(`${path}.$db ${props.$db} should be a string`)
        }
      } else if (field === '$id') {
        if (typeof props.$id !== 'string' && !Array.isArray(props.$id)) {
          if (path !== '' && typeof props.$id === 'object') {
            const allowed = checkAllowed(props.$id, new Set(['$field']))
            if (allowed !== true) {
              throw new Error(
                `${path}.$id is an object and with unallowed field ${allowed}, only $field is allowed in $id of nested ueries`
              )
            }

            continue
          }

          if (path !== '') {
            throw new Error(
              `$id ${props.$id} in a nested query should be a string, an array of strings or an object with $field reference`
            )
          } else {
            throw new Error(
              `$id ${props.$id} should be a string or an array of strings`
            )
          }
        }
      } else if (field === '$includeMeta') {
        // internal option
        continue
      } else if (field === '$alias') {
        if (typeof props.$alias !== 'string' && !Array.isArray(props.$alias)) {
          throw new Error(
            `${path}.$alias ${props.$alias} should be a string or an array of strings`
          )
        }
      } else if (field === '$version') {
        if (typeof props.$id !== 'string') {
          throw new Error(`$version should be a string`)
        }
      } else if (field === '$language') {
        if (props.$language === undefined) {
          delete props.$language
          continue
        }

        if (typeof props.$language !== 'string') {
          throw new Error(
            `$language ${props.$language} is unsupported, should be a string`
          )
        }

        const schema = client.schemas[props.$db || 'default']
        if (schema) {
          if (
            !schema.languages ||
            !schema.languages.includes(props.$language)
          ) {
            throw new Error(
              `$language ${
                props.$language
              } is unsupported, should be one of: ${schema.languages.join(
                ', '
              )}`
            )
          }
        }
      } else if (field === '$rawAncestors') {
        if (typeof props.$rawAncestors !== 'boolean') {
          throw new Error(`$rawAncestors should be a boolean value`)
        }
      } else if (field === '$all') {
        if (typeof props.$all !== 'boolean') {
          throw new Error(
            `Operator $all for ${path}.$all must be a boolean, got ${props.$all}`
          )
        }
      } else if (field === '$list') {
        validateList(props, client, props.$list, path)
      } else if (field === '$find') {
        validateFind(props, client, props.$find, path)
      } else {
        throw new Error(`
          Top level query operator ${field} is not supported. Did you mean one of the following supported top level query options?
            - $db
            - $id
            - $find
            - $list
            - $alias
            - $all
            - $version
            - $language
          `)
      }
    }
  }

  for (const field in props) {
    if (!field.startsWith('$')) {
      validateNested(extraQueries, client, props[field], path + '.' + field)
    }
  }
}
