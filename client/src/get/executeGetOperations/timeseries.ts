import { convertNow, isFork } from '@saulx/selva-query-ast-parser'
import squel from 'squel'
import { SelvaClient } from '../../'
import {
  FilterAST,
  Fork,
  GetOperationAggregate,
  GetOperationFind,
  GetOptions,
} from '../types'
import { FieldSchema } from '../../schema/types'
import { getNestedSchema, getTypeFromId } from '../utils'
import {
  executeNestedGetOperations,
  ExecContext,
  sourceFieldToDir,
  addMarker,
} from './'

const sq = squel.useFlavour('postgres')

function filterToExpr(
  fieldSchema: FieldSchema,
  filter: FilterAST
): squel.Expression {
  if (filter.$field === 'ts') {
    const v =
      typeof filter.$value === 'string' && filter.$value.startsWith('now')
        ? convertNow(filter.$value)
        : filter.$value
    return sq.expr().and(`"ts" ${filter.$operator} ?`, v)
  }

  const isObj = ['object', 'record'].includes(fieldSchema.type)
  const f = isObj ? `payload.${filter.$field}` : 'payload'

  if (!['=', '!=', '>', '<'].includes(filter.$operator)) {
    return sq.expr()
  }

  if (Array.isArray(filter.$value)) {
    let expr = sq.expr()
    for (const v of filter.$value) {
      expr = expr.or(`"${f}" ${filter.$operator} ?`, v)
    }
  } else {
    return sq.expr().and(`"${f}" ${filter.$operator} ?`, filter.$value)
  }
}

function getFields(path: string, fields: Set<string>, props: GetOptions): void {
  for (const k in props) {
    const newPath = path === '' ? k : path + '.' + k

    if (!k.startsWith('$')) {
      const p = props[k]
      if (typeof p === 'object') {
        getFields(newPath, fields, props)
      } else {
        fields.add(newPath)
      }
    }
  }
}

function forkToExpr(fieldSchema: FieldSchema, filter: Fork): squel.Expression {
  let expr = sq.expr()

  if (filter.$and) {
    for (const f of filter.$and) {
      expr = expr.and(toExpr(fieldSchema, f))
    }
  } else {
    for (const f of filter.$or) {
      expr = expr.or(toExpr(fieldSchema, f))
    }
  }

  return expr
}

function toExpr(
  fieldSchema: FieldSchema,
  filter: Fork | FilterAST
): squel.Expression {
  if (!filter) {
    return sq.expr()
  }

  if (isFork(filter)) {
    return forkToExpr(fieldSchema, filter)
  }

  return filterToExpr(fieldSchema, <FilterAST>filter)
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

  let sql = sq
    .select({
      autoQuoteTableNames: true,
      autoQuoteAliasNames: true,
      nameQuoteCharacter: '"',
    })
    .from(`${type}$${op.sourceField}`)
    .field('ts')
    .where('"nodeId" = ?', op.id)
    .where(toExpr(fieldSchema, op.filter))
    .limit(op.options.limit === -1 ? null : op.options.limit)
    .offset(op.options.offset === 0 ? null : op.options.offset)

  if (['object', 'record'].includes(fieldSchema.type)) {
    const fields: Set<string> = new Set()
    getFields('', fields, op.props)
    console.log('FIELDS', fields)
    for (const f of fields) {
      // const split = f.split('.').map((part) => "'" + part + "'")
      // let fieldStr = 'payload->'
      // for (let i = 0; i < split.length - 1; i++) {
      //   fieldStr += split[i] + '-> '
      // }
      // fieldStr + '->>' + split[split.length - 1]
      // sql = sql
      //   .field(fieldStr, f, {
      //     ignorePeriodsForFieldNameQuotes: true,
      //   })
    }

    sql = sql
      .field('payload')
      .order(
        op.options?.sort?.$field || '',
        op.options?.sort?.$order === 'asc'
          ? true
          : op.options?.sort?.$order === 'desc'
          ? false
          : null
      )
  } else {
    sql = sql
      .field('payload', 'value')
      .order(
        op.options?.sort?.$field ? 'payload' : '',
        op.options?.sort?.$order === 'asc'
          ? true
          : op.options?.sort?.$order === 'desc'
          ? false
          : null
      )
  }

  const params = sql.toParam({ numberedParametersStartAt: 1 })
  console.log('SQL', params)
  try {
    console.log('RESULT', await client.pg.query(params.text, params.values))
  } catch (e) {
    console.error('HMM', e)
  }
  return null
}
