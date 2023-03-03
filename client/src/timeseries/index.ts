import { QueryResult } from 'pg'
import BQConnection, { getTableName } from '../connection/pg'

import { FieldSchema } from '../schema/types'
import { convertNow, isFork } from '@saulx/selva-query-ast-parser'
import squel from 'squel'
import { constants, SelvaClient, ServerDescriptor } from '..'
import {
  FilterAST,
  Fork,
  GetOperationAggregate,
  GetOperationFind,
} from '../get/types'

export type TimeseriesContext = {
  nodeType: string
  field: string
  selectFields?: Set<string>
  fieldSchema?: FieldSchema
  startTime?: number
  endTime?: number
  order?: 'asc' | 'desc'
  limit?: number
  offset?: number
}

export const SELVA_TO_SQL_TYPE = {
  float: 'FLOAT64',
  boolean: 'BOOLEAN',
  number: 'FLOAT64',
  int: 'INT64',
  string: 'STRING',
  STRING: 'JSON', // because of localization
  json: 'JSON',
  id: 'STRING',
  digest: 'STRING',
  url: 'STRING',
  email: 'STRING',
  phone: 'STRING',
  geo: 'JSON',
  type: 'STRING',
  timestamp: 'INT64',
  reference: 'STRING',
  references: 'JSON',
  object: 'JSON',
  record: 'JSON',
  array: 'JSON',
}

const DEFAULT_QUERY_LIMIT = 500

// const sq = squel.useFlavour('postgres')
const sq = squel

function filterToExpr(
  exprCtx: TimeseriesContext,
  fieldSchema: FieldSchema,
  filter: FilterAST
): squel.Expression {
  // TODO: handle values by field schema type
  if (filter.$field === 'ts') {
    const v: number =
      typeof filter.$value === 'string' && filter.$value.startsWith('now')
        ? convertNow(filter.$value)
        : <number>filter.$value

    if (filter.$value !== 'now') {
      if (filter.$operator === '>') {
        exprCtx.startTime = !exprCtx.startTime
          ? v
          : Math.min(v, exprCtx.startTime)
      } else if (filter.$operator === '<') {
        exprCtx.endTime = !exprCtx.endTime ? v : Math.max(v, exprCtx.endTime)
      }
    }

    return sq.expr().and(`ts ${filter.$operator} TIMESTAMP_MILLIS(${v})`)
  }

  const isObj = ['object', 'record'].includes(fieldSchema.type)
  const f = isObj ? `payload.${filter.$field}` : 'payload'

  if (!['=', '!=', '>', '<'].includes(filter.$operator)) {
    return sq.expr()
  }

  if (Array.isArray(filter.$value)) {
    let expr = sq.expr()
    for (const v of filter.$value) {
      // if (isObj) {
      //   const split = f.split('.')
      //   const col = split[0]
      //   const path = split.slice(1).map((x) => `'${x}'::text`)
      //   const eStr = `jsonb_extract_path_text(${col}::jsonb, ${path.join(
      //     ', '
      //   )})`
      //   expr = expr.or(`${eStr} ${filter.$operator} ?`, v)
      // } else {
      //   expr = expr.or(`"${f}" ${filter.$operator} ?`, v)
      // }

      if (isObj) {
        expr = expr.or(`JSON_VALUE(${f}) ${filter.$operator} ?`, v)
      } else {
        expr = expr.or(`${f} ${filter.$operator} ?`, v)
      }
    }

    return expr
  } else {
    // if (isObj) {
    //   const split = f.split('.')
    //   const col = split[0]
    //   const path = split.slice(1).map((x) => `'${x}'::text`)
    //   const eStr = `jsonb_extract_path_text(${col}::jsonb, ${path.join(', ')})`

    //   return sq.expr().and(`${eStr} ${filter.$operator} ?`, filter.$value)
    // } else {
    //   return sq.expr().and(`"${f}" ${filter.$operator} ?`, filter.$value)
    // }

    if (isObj) {
      return sq
        .expr()
        .and(`JSON_VALUE(${f}) ${filter.$operator} ?`, filter.$value)
    } else {
      return sq.expr().and(`${f} ${filter.$operator} ?`, filter.$value)
    }
  }
}

function forkToExpr(
  exprCtx: TimeseriesContext,
  fieldSchema: FieldSchema,
  filter: Fork
): squel.Expression {
  let expr = sq.expr()

  if (filter.$and) {
    for (const f of filter.$and) {
      expr = expr.and(toExpr(exprCtx, fieldSchema, f))
    }
  } else {
    for (const f of filter.$or) {
      expr = expr.or(toExpr(exprCtx, fieldSchema, f))
    }
  }

  return expr
}

function toExpr(
  exprCtx: TimeseriesContext,
  fieldSchema: FieldSchema,
  filter: Fork | FilterAST
): squel.Expression {
  if (!filter) {
    return sq.expr()
  }

  if (isFork(filter)) {
    return forkToExpr(exprCtx, fieldSchema, filter)
  }

  return filterToExpr(exprCtx, fieldSchema, <FilterAST>filter)
}

async function getInsertQueue(
  client: SelvaClient,
  exprCtx: TimeseriesContext,
  id: string,
  field: string
): Promise<any[]> {
  try {
    return (
      await client.redis.lrange(
        { type: 'timeseriesQueue' },
        'timeseries_inserts',
        0,
        -1
      )
    )
      .map((e) => JSON.parse(e))
      .filter((e) => {
        return (
          e.type === 'insert' &&
          e.context.nodeId === id &&
          e.context.nodeType === exprCtx.nodeType &&
          e.context.field === field &&
          (!exprCtx.startTime ? true : e.context.ts >= exprCtx.startTime) &&
          (!exprCtx.endTime ? true : e.context.ts <= exprCtx.endTime)
        )
      })
  } catch (_e) {
    return []
  }
}

async function runSelect<T>(
  tsCtx: TimeseriesContext,
  client: SelvaClient,
  op: GetOperationFind | GetOperationAggregate,
  where: squel.Expression
): Promise<QueryResult<T>> {
  let limit = tsCtx.limit
  // TODO: offset
  // let offset = tsCtx.offset

  const result = await execTimeseries(tsCtx, client, op, {
    where,
    limit,
    offset: 0,
  })

  return result
}

async function execTimeseries(
  tsCtx: TimeseriesContext,
  client: SelvaClient,
  op: GetOperationFind | GetOperationAggregate,
  queryOptions: {
    where: squel.Expression
    limit: number
    offset: number
  }
): Promise<any> {
  const { fieldSchema, nodeType } = tsCtx

  let sql = sq
    .select({
      // autoQuoteTableNames: true,
      // autoQuoteAliasNames: true,
      // nameQuoteCharacter: '"',
      autoQuoteTableNames: false,
      autoQuoteAliasNames: false,
      autoQuoteFieldNames: false,
    })
    .from(`selva_timeseries.${getTableName(tsCtx)}`)
    .field('ts', '_ts')
    .where('nodeId = ?', op.id)
    .where(queryOptions.where)
    .limit(queryOptions.limit <= 0 ? null : queryOptions.limit)

  let isObj = false
  if (
    tsCtx?.selectFields?.size &&
    ['object', 'record'].includes(fieldSchema.type)
  ) {
    isObj = true
    for (const f of tsCtx.selectFields) {
      //   const split = f.split('.')
      //   const path = split.map((x) => `'${x}'::text`)
      //   const eStr = `jsonb_extract_path_text(payload::jsonb, ${path.join(', ')})`
      //   sql = sql.field(eStr, f)
      sql.field(`payload.${f}`, f)
    }
  } else {
    sql = sql.field('payload', 'value')
  }

  sql = sql.order('ts', tsCtx.order === 'asc')

  const params = sql.toParam()
  console.log('SQL', params, 'tsCtx', tsCtx)

  const result: any[] = await client.pg.pg.execute(params.text, params.values)
  console.log('RAW RESULTS', result)
  if (['object', 'record'].includes(tsCtx?.fieldSchema?.type)) {
    result.forEach((row) => {
      try {
        row.value = JSON.parse(row.value)
      } catch (e) {}
      row._ts = row._ts.value
    })
  } else {
    result.forEach((row) => {
      row._ts = row._ts.value
    })
  }

  console.log('SQL RESULT', JSON.stringify(result, null, 2))
  return { rows: result }
}

export class TimeseriesClient {
  private client: SelvaClient
  public pg: BQConnection

  private isConnected: boolean = false

  constructor(client: SelvaClient) {
    // TODO: credentials
    this.client = client
    this.pg = new BQConnection()
  }

  async connect() {
    if (this.isConnected) {
      return
    }

    this.isConnected = true
  }

  disconnect() {
    // TODO disconnect automatically after a while?
    if (!this.isConnected) {
      return
    }

    this.isConnected = false
  }

  public async execute(
    tsCtx: string,
    query: string,
    params: unknown[]
  ): Promise<any[]> {
    return this.pg.execute(query, params)
  }

  async ensureTableExists(tsCtx: TimeseriesContext): Promise<void> {
    await this.pg.createTable(tsCtx)
  }

  public async insert(
    tsCtx: TimeseriesContext,
    entry: { nodeId: string; ts: number; payload: any }
  ): Promise<any[]> {
    try {
      await this.ensureTableExists(tsCtx)
    } catch (e) {
      // console.error('TABLE CREATION ERROR', e)
    }

    try {
      // const res = await this.pg.insert(tsCtx, [entry])
      const res = await this.pg.insertStream(tsCtx, [entry])
      return res
    } catch (e) {
      console.error(e, JSON.stringify(e))
      throw e
    }
  }

  public async select<T>(
    tsCtx: TimeseriesContext,
    op: GetOperationFind | GetOperationAggregate
  ): Promise<QueryResult<T>> {
    // order is important
    const where = toExpr(tsCtx, tsCtx.fieldSchema, op.filter)

    // apply context defaults for limiting result set size
    if (!(tsCtx.startTime && tsCtx.endTime) && tsCtx.limit === -1) {
      tsCtx.limit = DEFAULT_QUERY_LIMIT
    }

    return runSelect(tsCtx, this.client, op, where)
  }
}
