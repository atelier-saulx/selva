import { Find, GetOperationFind, GetOptions, Sort } from '../types'
import { createAst, optimizeTypeFilters } from '@saulx/selva-query-ast-parser'
import { getNestedSchema } from '../utils'
import { SelvaClient } from '../..'

const createFindOperation = (
  client: SelvaClient,
  db: string,
  find: Find,
  props: GetOptions,
  id: string,
  field: string,
  single: boolean,
  limit: number = single ? 1 : -1,
  offset: number = 0,
  sort?: Sort | Sort[],
  isNested?: boolean
): GetOperationFind => {
  const fieldSchema = getNestedSchema(
    client.schemas[db],
    id,
    <string>props.$field || field.substr(1)
  )

  const isTimeseries = fieldSchema && fieldSchema.timeseries

  const findOperation: GetOperationFind = {
    type: 'find',
    id,
    props,
    single,
    field: field.substr(1),
    sourceField: field.substr(1),
    recursive: find.$recursive,
    options: {
      limit,
      offset,
      sort: Array.isArray(sort) ? sort[0] : sort || undefined,
    },
    isNested,
    isTimeseries,
  }

  if (find.$traverse) {
    if (typeof find.$traverse === 'string') {
      findOperation.sourceField = find.$traverse
    } else if (Array.isArray(find.$traverse)) {
      findOperation.inKeys = find.$traverse
    }
  }

  if (find.$filter) {
    const ast = createAst(find.$filter)

    if (ast) {
      optimizeTypeFilters(ast)
      findOperation.filter = ast
    }
  }

  if (find.$find) {
    findOperation.options.limit = -1
    findOperation.options.offset = 0
    findOperation.nested = createFindOperation(
      client,
      db,
      find.$find,
      props,
      '',
      field,
      single,
      limit,
      offset,
      find.$find.$find ? undefined : sort,
      true
    )
  }

  return findOperation
}

export default createFindOperation
