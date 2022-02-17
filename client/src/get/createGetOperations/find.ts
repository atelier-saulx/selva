import { Find, GetOperationFind, GetOptions, Sort } from '../types'
import { createAst, optimizeTypeFilters } from '@saulx/selva-query-ast-parser'
import { getNestedSchema, isTraverseByType } from '../utils'
import { Schema, SelvaClient } from '../..'

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
  isNested?: boolean,
  passedOnSchema?: Schema
): GetOperationFind => {
  const fieldSchema = getNestedSchema(
    passedOnSchema || client.schemas[db],
    id,
    <string>find.$traverse || field.slice(1)
  )

  const isTimeseries = fieldSchema && fieldSchema.timeseries

  const findOperation: GetOperationFind = {
    type: 'find',
    id,
    props,
    single,
    field: field.slice(1),
    sourceField: field.slice(1),
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
    } else if (isTraverseByType(find.$traverse)) {
      findOperation.byType = find.$traverse
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
      true,
      passedOnSchema
    )
  }

  return findOperation
}

export default createFindOperation
