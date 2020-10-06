import { SelvaClient } from '../'
import { GetOperation, GetOptions } from './types'
import { getNestedSchema } from './utils'
import { createAst } from '@saulx/selva-query-ast-parser'
import { getTypeFromId } from './utils'

// make this rly nice

export default function createGetOperations(
  client: SelvaClient,
  props: GetOptions,
  id: string,
  field: string,
  ops: GetOperation[] = []
): GetOperation[] {
  if (props.$value) {
    ops.push({
      type: 'value',
      field: field.substr(1),
      value: props.$value
    })
  } else if (props.$id && field) {
    ops.push({
      type: 'nested_query',
      field: field.substr(1),
      props
    })
  } else if (Array.isArray(props)) {
    ops.push({
      type: 'array_query',
      id,
      field: field.substr(1),
      props
    })
  } else if (props.$list) {
    if (props.$list === true) {
      ops.push({
        type: 'find',
        id: id.padEnd(10, '\0'),
        props,
        field: field.substr(1),
        sourceField: field.substr(1),
        options: {
          limit: -1, // no limit
          offset: 0
        }
      })
    } else if (props.$list.$find) {
      // TODO: $find in $list
      const allwaysWant: GetOperation = {
        type: 'find',
        id: id.padEnd(10, '\0'),
        props,
        field: field.substr(1),
        sourceField: field.substr(1),
        options: {
          limit: props.$list.$find.$find ? -1 : props.$list.$limit || -1,
          offset: props.$list.$find.$find ? 0 : props.$list.$offset || 0,
          sort: props.$list.$find.$find
            ? undefined
            : Array.isArray(props.$list.$sort)
            ? props.$list.$sort[0]
            : props.$list.$sort || undefined
        }
      }

      if (props.$list.$find.$traverse) {
        if (typeof props.$list.$find.$traverse === 'string') {
          allwaysWant.sourceField = props.$list.$find.$traverse
        } else if (Array.isArray(props.$list.$find.$traverse)) {
          allwaysWant.inKeys = props.$list.$find.$traverse
        }
      }

      if (props.$list.$find.$filter) {
        const ast = createAst(props.$list.$find.$filter)
        // const rpn = createRpn(props.$list.$find.$filter, lang)
        if (ast) {
          allwaysWant.filter = ast
        }
      }

      // props.$list

      if (props.$list.$find.$find) {
        const makeitnice = (myFind: any): GetOperation => {
          if (props.$list !== true) {
            const flapperPants: GetOperation = {
              type: 'find',
              id: '',
              props,
              field: field.substr(1),
              sourceField: field.substr(1),
              options: {
                limit: props.$list.$limit || -1,
                offset: props.$list.$offset || 0,
                sort: Array.isArray(props.$list.$sort)
                  ? props.$list.$sort[0]
                  : props.$list.$sort || undefined
              }
            }

            if (myFind.$traverse) {
              if (typeof myFind.$traverse === 'string') {
                flapperPants.sourceField = myFind.$traverse
              } else if (Array.isArray(myFind.$traverse)) {
                flapperPants.inKeys = myFind.$traverse
              }
            }

            if (myFind.$filter) {
              const ast = createAst(myFind.$filter)
              if (ast) {
                flapperPants.filter = ast
              }
            }

            if (myFind.$find) {
              flapperPants.nested = makeitnice(myFind.$find)
            }

            return flapperPants
          }
        }

        allwaysWant.nested = makeitnice(props.$list.$find.$find)
      }

      ops.push(allwaysWant)
    } else {
      ops.push({
        type: 'find',
        id: id.padEnd(10, '\0'),
        props,
        field: field.substr(1),
        sourceField: <string>props.$field || field.substr(1),
        options: {
          limit: props.$list.$limit || -1,
          offset: props.$list.$offset || 0,
          sort: Array.isArray(props.$list.$sort)
            ? props.$list.$sort[0]
            : props.$list.$sort || undefined
        }
      })
    }
  } else if (props.$find) {
    // inKeys

    console.log('hello this', field)

    const myFind: GetOperation = {
      type: 'find',
      id: id.padEnd(10, '\0'),
      props,
      single: true,
      field: field.substr(1),
      sourceField:
        props.$find.$traverse && !Array.isArray(props.$find.$traverse)
          ? <string>props.$find.$traverse
          : field.substr(1),
      options: {
        limit: 1,
        offset: 0
      }
    }

    if (props.$find.$filter) {
      const ast = createAst(props.$find.$filter)
      if (ast) {
        myFind.filter = ast
      }
    }

    ops.push(myFind)

    console.dir({ myFind }, { depth: 10 })

    // TODO shitty
  } else if (
    props.$field &&
    typeof props.$field === 'object' &&
    (<any>props.$field).value
  ) {
    // TODO
  } else if (props.$field) {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: <string[]>props.$field
    })
  } else if (props.$all) {
    // TODO: proper $db support, well, everywhere basically not just here
    const schema = client.schemas.default
    if (field === '') {
      const type = getTypeFromId(schema, id)
      const typeSchema = type === 'root' ? schema.rootType : schema.types[type]

      if (!typeSchema) {
        return
      }

      for (const key in typeSchema.fields) {
        if (
          key !== 'children' &&
          key !== 'parents' &&
          key !== 'ancestors' &&
          key !== 'descendants'
        ) {
          if (props[key] === undefined) {
            ops.push({
              type: 'db',
              id,
              field: key,
              sourceField: key
            })
          } else if (props[key] === false) {
            // do nothing
          } else {
            createGetOperations(client, props[key], id, field + '.' + key, ops)
          }
        }
      }

      return
    }

    const fieldSchema = getNestedSchema(schema, id, field.slice(1))
    if (!fieldSchema) {
      return
    }

    if (fieldSchema.type === 'object') {
      for (const key in fieldSchema.properties) {
        if (props[key] === undefined) {
          ops.push({
            type: 'db',
            id,
            field: field.slice(1) + '.' + key,
            sourceField: field.slice(1) + '.' + key
          })
        } else if (props[key] === false) {
          // do nothing
        } else {
          createGetOperations(client, props[key], id, field + '.' + key, ops)
        }
      }
    } else if (fieldSchema.type === 'record' || fieldSchema.type === 'text') {
      // basically this is the same as: `field: true`
      ops.push({
        type: 'db',
        id,
        field: field.slice(1),
        sourceField: field.slice(1)
      })
    }
  } else if (props.$default) {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: field.substr(1),
      default: props.$default
    })
  } else if (typeof props === 'object') {
    for (const key in props) {
      if (key.startsWith('$')) {
        continue
      }
      createGetOperations(client, props[key], id, field + '.' + key, ops)
    }
  } else {
    ops.push({
      type: 'db',
      id,
      field: field.substr(1),
      sourceField: field.substr(1)
    })
  }

  return ops
}
