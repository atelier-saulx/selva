import { Aggregate, GetOperationAggregate, GetOptions, Sort } from '../types'
import { createAst, optimizeTypeFilters } from '@saulx/selva-query-ast-parser'
import createFindOperation from './find'
import { isTraverseByType } from '../utils'

const createAggregateOperation = (
  aggregate: Aggregate,
  props: GetOptions,
  id: string,
  field: string,
  limit?: number,
  offset?: number,
  sort?: Sort | Sort[]
): GetOperationAggregate => {
  sort = sort || aggregate.$sort
  const op: GetOperationAggregate = {
    type: 'aggregate',
    id,
    props,
    field: field.substr(1),
    sourceField: field.substr(1),
    recursive: !!aggregate.$recursive,
    options: {
      limit: limit || aggregate.$limit || -1,
      offset: offset || aggregate.$offset || 0,
      sort: Array.isArray(sort) ? sort[0] : sort || undefined,
    },
    function:
      typeof aggregate.$function === 'string'
        ? { name: aggregate.$function }
        : { name: aggregate.$function.$name, args: aggregate.$function.$args },
  }

  if (aggregate.$traverse) {
    if (typeof aggregate.$traverse === 'string') {
      op.sourceField = aggregate.$traverse
    } else if (Array.isArray(aggregate.$traverse)) {
      op.inKeys = aggregate.$traverse
    } else if (isTraverseByType(aggregate.$traverse)) {
      op.byType = aggregate.$traverse
    }
  }

  if (aggregate.$filter) {
    const ast = createAst(aggregate.$filter)

    if (ast) {
      optimizeTypeFilters(ast)
      op.filter = ast
    }
  }

  if (aggregate.$find) {
    op.nested = createFindOperation(
      aggregate.$find,
      props,
      '',
      field,
      false,
      limit,
      offset,
      aggregate.$find.$find ? undefined : sort,
      true
    )
  }

  return op
}

export default createAggregateOperation
