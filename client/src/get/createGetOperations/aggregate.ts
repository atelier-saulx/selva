import { Find, GetOperationAggregate, GetOptions, Sort } from '../types'
import { createAst, optimizeTypeFilters } from '@saulx/selva-query-ast-parser'

const createAggregateOperation = (
  find: Find,
  props: GetOptions,
  id: string,
  field: string,
  isNested?: boolean
): GetOperationAggregate => {
  const op: GetOperationAggregate = {
    type: 'aggregate',
    id,
    props,
    field: field.substr(1),
    sourceField: field.substr(1),
    isNested,
  }

  if (find.$traverse) {
    if (typeof find.$traverse === 'string') {
      op.sourceField = find.$traverse
    } else if (Array.isArray(find.$traverse)) {
      op.inKeys = find.$traverse
    }
  }

  if (find.$filter) {
    const ast = createAst(find.$filter)

    if (ast) {
      optimizeTypeFilters(ast)
      op.filter = ast
    }
  }

  return op
}

export default createAggregateOperation
