import { isFork } from '@saulx/selva-query-ast-parser'
import squel from 'squel'
import { SelvaClient } from '../../'
import {
  FilterAST,
  Fork,
  GetOperationAggregate,
  GetOperationFind,
} from '../types'
import { getNestedSchema, getTypeFromId } from '../utils'
import {
  executeNestedGetOperations,
  ExecContext,
  sourceFieldToDir,
  addMarker,
} from './'

function filterToExpr(filter: FilterAST): squel.Expression {
  if (!['=', '!=', '>', '<'].includes(filter.$operator)) {
    return squel.expr()
  }

  if (Array.isArray(filter.$value)) {
    let expr = squel.expr()
    for (const v of filter.$value) {
      expr = expr.or(`\`${filter.$field}\` ${filter.$operator} ?`, v)
    }
  } else {
    return squel
      .expr()
      .and(`\`${filter.$field}\` ${filter.$operator} ?`, filter.$value)
  }
}

function forkToExpr(filter: Fork): squel.Expression {
  let expr = squel.expr()

  if (filter.$and) {
    for (const f of filter.$and) {
      expr = expr.or(toExpr(f))
    }
  } else {
    for (const f of filter.$and) {
      expr = expr.and(toExpr(f))
    }
  }

  return expr
}

function toExpr(filter: Fork | FilterAST): squel.Expression {
  if (!filter) {
    return squel.expr()
  }

  if (isFork(filter)) {
    return forkToExpr(filter)
  }

  return filterToExpr(<FilterAST>filter)
}

export default async function execTimeseries(
  client: SelvaClient,
  op: GetOperationFind | GetOperationAggregate,
  lang: string,
  ctx: ExecContext
): Promise<any> {
  console.log('IS TIMESERIES', JSON.stringify(op, null, 2))
  const fieldSchema = getNestedSchema(
    client.schemas[ctx.db],
    op.id,
    <string>op.sourceField
  )
  // TODO: use the field schema to determine how to treat field path in filters
  console.log('FIELD SCHEMA', fieldSchema)
  const type = getTypeFromId(client.schemas[ctx.db], op.id)

  const sql = squel
    .select({
      autoQuoteFieldNames: true,
      autoQuoteTableNames: true,
      autoQuoteAliasNames: true,
    })
    .from(`${type}_${op.sourceField}`)
    .field('ts')
    .field('payload')
    .where("`nodeId` = '?'", op.id)
    .where(toExpr(op.filter))
    .limit(op.options.limit === -1 ? null : op.options.limit)
    .offset(op.options.offset === 0 ? null : op.options.offset)
    .order(
      op.options?.sort?.$field || '',
      op.options?.sort?.$order === 'asc'
        ? true
        : op.options?.sort?.$order === 'desc'
        ? false
        : null
    )
    .toParam()
  console.log('SQL', sql)
  return null
}
