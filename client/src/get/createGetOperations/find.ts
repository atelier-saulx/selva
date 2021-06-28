import { Find, GetOperationFind, GetOptions, Sort } from '../types'
import { createAst, optimizeTypeFilters } from '@saulx/selva-query-ast-parser'
import createAggregateOperation from './aggregate'

const createFindOperation = (
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
  } else if (find.$aggregate) {
    findOperation.options.limit = -1
    findOperation.options.offset = 0
    findOperation.nested = createAggregateOperation(
      find.$find,
      props,
      '',
      field,
      limit,
      offset,
      find.$find.$find ? undefined : sort
    )
  }

  return findOperation
}

export default createFindOperation
