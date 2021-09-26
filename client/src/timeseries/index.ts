import { QueryResult } from 'pg'
import PGConnection from '../connection/pg'
import TimeseriesCache from './timeseriesCache'
import { PG } from '../connection/pg/client'

import { convertNow, isFork } from '@saulx/selva-query-ast-parser'
import squel from 'squel'
import { SelvaClient, ServerDescriptor } from '..'
import {
  FilterAST,
  Fork,
  GetOperationAggregate,
  GetOperationFind,
  GetOptions,
} from '../get/types'
import { FieldSchema } from '../schema/types'
import {
  getNestedField,
  getNestedSchema,
  getTypeFromId,
  setNestedResult,
} from '../get/utils'
import {
  executeNestedGetOperations,
  ExecContext,
  sourceFieldToDir,
  addMarker,
  executeGetOperation,
} from '../get/executeGetOperations'

export type TimeseriesContext = {
  nodeType: string
  field: string
  fieldSchema?: FieldSchema
  startTime?: number
  endTime?: number
  order?: 'asc' | 'desc'
  limit?: number
  offset?: number
}

const DEFAULT_QUERY_LIMIT = 500

const sq = squel.useFlavour('postgres')

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

    return sq.expr().and(`"ts" ${filter.$operator} to_timestamp(${v} / 1000.0)`)
  }

  const isObj = ['object', 'record'].includes(fieldSchema.type)
  const f = isObj ? `payload.${filter.$field}` : 'payload'

  if (!['=', '!=', '>', '<'].includes(filter.$operator)) {
    return sq.expr()
  }

  if (Array.isArray(filter.$value)) {
    let expr = sq.expr()
    for (const v of filter.$value) {
      if (isObj) {
        const split = f.split('.')
        const col = split[0]
        const path = split.slice(1).map((x) => `'${x}'::text`)
        const eStr = `jsonb_extract_path_text(${col}::jsonb, ${path.join(
          ', '
        )})`

        expr = expr.or(`${eStr} ${filter.$operator} ?`, v)
      } else {
        expr = expr.or(`"${f}" ${filter.$operator} ?`, v)
      }
    }

    return expr
  } else {
    if (isObj) {
      const split = f.split('.')
      const col = split[0]
      const path = split.slice(1).map((x) => `'${x}'::text`)
      const eStr = `jsonb_extract_path_text(${col}::jsonb, ${path.join(', ')})`

      return sq.expr().and(`${eStr} ${filter.$operator} ?`, filter.$value)
    } else {
      return sq.expr().and(`"${f}" ${filter.$operator} ?`, filter.$value)
    }
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

async function execTimeseries(
  pgDescriptor: string | ServerDescriptor,
  tsCtx: TimeseriesContext,
  client: SelvaClient,
  op: GetOperationFind | GetOperationAggregate,
  queryOptions: {
    where: squel.Expression
    shard: number | string
    limit: number
    offset: number
  }
): Promise<any> {
  const { fieldSchema, nodeType } = tsCtx

  let sql = sq
    .select({
      autoQuoteTableNames: true,
      autoQuoteAliasNames: true,
      nameQuoteCharacter: '"',
    })
    .from(`${nodeType}$${op.sourceField}$0`)
    .field('ts')
    .where('"nodeId" = ?', op.id)
    .where(queryOptions.where)
    .limit(queryOptions.limit === -1 ? null : queryOptions.limit)
    .offset(queryOptions.offset === 0 ? null : queryOptions.offset)

  const fields: Set<string> = new Set()
  let isObj = false
  if (['object', 'record'].includes(fieldSchema.type)) {
    isObj = true
    getFields('', fields, op.props)
    // TODO: goddamn json syntax
    // for (const f of fields) {
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
    // }

    sql = sql.field('payload')
  } else {
    sql = sql.field('payload', 'value')
  }

  const insertQueue = await getInsertQueue(client, tsCtx, op.id, tsCtx.field)

  if (insertQueue.length > 0) {
    const insertQueueSqlValues = insertQueue.map((e) => {
      if (['object', 'record'].includes(e.context.fieldSchema.type)) {
        return `(to_timestamp(${e.context.ts} / 1000.0), '${JSON.stringify(
          e.context.payload
        )}'::jsonb)`
      } else if (e.context.fieldSchema.type === 'string') {
        return `(to_timestamp(${e.context.ts} / 1000.0), '${e.context.payload}')`
      } else {
        return `(to_timestamp(${e.context.ts} / 1000.0), ${e.context.payload})`
      }
    })

    let qSql = sq
      .select({
        autoQuoteTableNames: false,
        autoQuoteAliasNames: true,
        nameQuoteCharacter: '"',
      })
      .from(`(VALUES ${insertQueueSqlValues.join(', ')}) AS t (ts, payload)`)
      .field('ts')
      .where(queryOptions.where)

    if (['object', 'record'].includes(fieldSchema.type)) {
      qSql = qSql.field('payload')
    } else {
      qSql = qSql.field('payload', 'value')
    }

    qSql = qSql.order('ts', tsCtx.order === 'asc')

    sql = sql.union_all(qSql)
  } else {
    sql = sql.order('ts', tsCtx.order === 'asc')
  }

  const params = sql.toParam({ numberedParametersStartAt: 1 })
  console.log('SQL', params, 'SELECTOR', tsCtx)

  const result: QueryResult<any> = await client.pg.pg.execute(
    pgDescriptor,
    params.text,
    params.values
  )

  return { fields, result }
}

export class TimeseriesClient {
  private client: SelvaClient
  public pg: PGConnection
  public tsCache: TimeseriesCache

  private isConnected: boolean = false

  constructor(client: SelvaClient) {
    // TODO: credentials
    this.client = client
    this.pg = new PGConnection({ user: 'postgres', password: 'baratta' })
    this.tsCache = new TimeseriesCache(this.client)
  }

  async connect() {
    if (this.isConnected) {
      return
    }

    await this.tsCache.subscribe()
    this.isConnected = true
  }

  disconnect() {
    // TODO disconnect automatically after a while?
    if (!this.isConnected) {
      return
    }

    this.tsCache.unsubscribe()
    this.isConnected = false
  }

  public getMinInstance(): PG {
    const instances = Object.keys(this.tsCache.instances)
    if (!instances.length) {
      return null
    }

    let minId = instances[0]
    let minVal = this.tsCache.instances[instances[0]].meta
      .totalRelationSizeBytes
    for (let i = 1; i < instances.length; i++) {
      const id = instances[i]
      const { meta } = this.tsCache.instances[id]

      if (meta.totalRelationSizeBytes < minVal) {
        minId = id
        minVal = meta.totalRelationSizeBytes
      }
    }

    return this.pg.getClient(minId)
  }

  public hasTimeseries(selector: TimeseriesContext): boolean {
    const tsName = `${selector.nodeType}$${selector.field}`
    return !!this.tsCache.index[tsName]
  }

  public async execute<T>(
    selector: string,
    query: string,
    params: unknown[]
  ): Promise<QueryResult<T>> {
    const [type, field, shard] = selector.split('$')

    const shards = this.tsCache.index[`${type}$${field}`]
    if (!shards || !shards[0]) {
      throw new Error(`Timeseries shard ${selector} does not exist`)
    }

    const pgInstance = shards[shard].descriptor
    return this.pg.execute(pgInstance, query, params)
  }

  public async insert<T>(
    selector: TimeseriesContext,
    query: string,
    params: unknown[]
  ): Promise<QueryResult<T>> {
    // TODO: real logic for selecting shard
    const tsName = `${selector.nodeType}$${selector.field}`
    const shards = this.tsCache.index[tsName]
    if (!shards || !shards[0]) {
      // TODO: implicitly create? or error and create it in the catch?
      console.log('HELLO', JSON.stringify(this.tsCache.index, null, 2))
      throw new Error(`INSERT: Timeseries ${tsName} does not exist`)
    }

    const pgInstance = shards[0].descriptor
    return this.pg.execute(pgInstance, query, params)
  }

  private selectShards(
    selector: TimeseriesContext
  ): { ts: number; descriptor: ServerDescriptor }[] {
    // if we have a startTime, start iterating from beginning to find the shard to start
    // if we have an endTime, keep iterating till you go over
    //
    // if we only have endTime then iterate from the end to find where to end

    const tsName = `${selector.nodeType}$${selector.field}`
    const shards = this.tsCache.index[tsName]
    if (!shards || !shards[0]) {
      // TODO: implicitly create? or error and create it in the catch?
      throw new Error(`SELECT: Timeseries ${tsName} does not exist`)
    }

    let shardList = Object.keys(shards)
      .map((k) => Number(k))
      .sort((a, b) => a - b)
      .map((ts) => {
        return { ts, descriptor: shards[ts].descriptor }
      })

    if (selector.startTime) {
      let startIdx = 0
      let endIdx = shardList.length - 1
      for (let i = 1; i < shardList.length; i++) {
        const shard = shardList[i]

        if (shard.ts > selector.startTime) {
          break
        }

        startIdx = i
      }

      if (selector.endTime) {
        endIdx = startIdx
        for (let i = startIdx + 1; i++; i < shardList.length) {
          const shard = shardList[i]

          if (shard.ts > selector.endTime) {
            break
          }

          endIdx = i
        }
      }

      shardList = shardList.slice(startIdx, endIdx + 1)
      // FIXME: ?
      // if (selector.order === 'desc') {
      //   shardList.reverse()
      // } else {
      //   // timestamp between two values makes sense in default ascending order
      //   selector.order = 'asc'
      // }
    } else if (selector.endTime) {
      let endIdx = shardList.length - 1

      for (let i = shardList.length - 1; i >= 0; i--) {
        const shard = shardList[i]
        if (selector.endTime > shard.ts) {
          break
        }

        endIdx = i
      }

      shardList = shardList.slice(0, endIdx + 1)
    }

    if (selector.order === 'desc') {
      shardList.reverse()
    }

    return shardList
  }

  // TODO: the query here needs to be a higher level consruct than SQL, because we need to adjust query contents based on shard targeted
  public async select<T>(
    selector: TimeseriesContext,
    op: GetOperationFind | GetOperationAggregate
  ): Promise<QueryResult<T>> {
    const shards = this.selectShards(selector)
    const where = toExpr(selector, selector.fieldSchema, op.filter)

    // apply context defaults for limiting result set size
    if (!(selector.startTime && selector.endTime) && selector.limit === -1) {
      selector.limit = DEFAULT_QUERY_LIMIT
    }

    // TODO: real logic for selecting shard
    const { result, fields } = await execTimeseries(
      shards[0].descriptor,
      selector,
      this.client,
      op,
      {
        shard: 0,
        where,
        limit: op.options.limit,
        offset: op.options.offset,
      }
    )

    // TODO: combine resuls of several shards
    // remember when running on several shards that limit/offset parameters need to be
    // adjusted by shard (only first shard as them, or adjust on amount of rows returned
    // always query shards in order based on order options (asc/desc)
    return result.rows.map((row) => {
      if (fields.size) {
        const r = {}
        for (const f of fields) {
          setNestedResult(r, f, getNestedField(row, `payload.${f}`))
        }

        return r
      }

      return row.value
    })
  }
}
