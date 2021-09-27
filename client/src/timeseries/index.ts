import { QueryResult } from 'pg'
import PGConnection from '../connection/pg'
import TimeseriesCache from './timeseriesCache'
import { PG } from '../connection/pg/client'

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

export type Shard = {
  ts: number
  descriptor: ServerDescriptor
  size: { relationSizeBytes: number; tableSizeBytes: number }
}

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
  float: 'DOUBLE PRECISION',
  boolean: 'BOOLEAN',
  number: 'DOUBLE PRECISION',
  int: 'integer',
  string: 'text',
  text: 'jsonb', // because of localization
  json: 'jsonb',
  id: 'text',
  digest: 'text',
  url: 'text',
  email: 'text',
  phone: 'text',
  geo: 'jsonb',
  type: 'text',
  timestamp: 'TIMESTAMP',
  reference: 'test',
  references: 'JSONB',
  object: 'JSONB',
  record: 'JSONB',
  array: 'JSONB',
}

// const MAX_SHARD_SIZE_BYTES = 1e9 // 1 GB
const MAX_SHARD_SIZE_BYTES = 1

const mkNewShard = () => {
  return Date.now() + 2 * 60 * 1e3 // 2 minutes in the future
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
  shards: Shard[],
  client: SelvaClient,
  op: GetOperationFind | GetOperationAggregate,
  where: squel.Expression
): Promise<QueryResult<T>> {
  let limit = tsCtx.limit
  // TODO: offset
  // let offset = tsCtx.offset
  let hasLimit = limit > 0

  let i = 0

  let result: QueryResult<T>
  if (tsCtx.order === 'desc') {
    result = await queryInsertQueue(shards[0].descriptor, tsCtx, client, op, {
      shard: shards[0].ts,
      where,
      limit: limit,
      offset: 0,
    })
  } else {
    i = 1
    result = await execTimeseries(shards[0].descriptor, tsCtx, client, op, {
      shard: shards[0].ts,
      where,
      limit: limit,
      offset: 0,
    })
  }

  limit -= result.rows.length

  if (limit < 0) {
    result.rows = result.rows.slice(0, limit)
    return result
  }

  for (; i < shards.length; i++) {
    const { ts, descriptor } = shards[i]

    const res = await execTimeseries(descriptor, tsCtx, client, op, {
      shard: ts,
      where,
      limit: limit,
      offset: 0,
    })

    for (const row of res.rows) {
      result.rows.push(row)
      limit--

      if (hasLimit && limit <= 0) {
        break
      }
    }
  }

  if (tsCtx.order === 'asc' && i >= shards.length) {
    const res = await queryInsertQueue(
      shards[0].descriptor,
      tsCtx,
      client,
      op,
      {
        shard: shards[0].ts,
        where,
        limit: limit,
        offset: 0,
      }
    )

    for (const row of res.rows) {
      result.rows.push(row)
      limit--

      if (hasLimit && limit <= 0) {
        break
      }
    }
  }

  return result
}

async function queryInsertQueue<T>(
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
  const { fieldSchema } = tsCtx

  const insertQueue = await getInsertQueue(client, tsCtx, op.id, tsCtx.field)
  if (!insertQueue.length) {
    return { rows: [] }
  }

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

  let sql = sq
    .select({
      autoQuoteTableNames: false,
      autoQuoteAliasNames: true,
      nameQuoteCharacter: '"',
    })
    .from(`(VALUES ${insertQueueSqlValues.join(', ')}) AS t (ts, payload)`)
    .field('ts')
    .where(queryOptions.where)
  if (['object', 'record'].includes(fieldSchema.type)) {
    sql = sql.field('payload')
  } else {
    sql = sql.field('payload', 'value')
  }
  sql = sql.order('ts', tsCtx.order === 'asc')

  const params = sql.toParam({ numberedParametersStartAt: 1 })

  const result: QueryResult<T> = await client.pg.pg.execute(
    pgDescriptor,
    params.text,
    params.values
  )

  return result
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
    .from(`${nodeType}$${op.sourceField}$${queryOptions.shard}`)
    .field('ts')
    .where('"nodeId" = ?', op.id)
    .where(queryOptions.where)
    .limit(queryOptions.limit <= 0 ? null : queryOptions.limit)
    .offset(queryOptions.offset <= 0 ? null : queryOptions.offset)

  let isObj = false
  if (['object', 'record'].includes(fieldSchema.type)) {
    isObj = true
    // TODO: goddamn json syntax
    // for (const f of tsCtx.selectFields) {
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

  sql = sql.order('ts', tsCtx.order === 'asc')

  const params = sql.toParam({ numberedParametersStartAt: 1 })
  console.log('SQL', params, 'tsCtx', tsCtx)

  const result: QueryResult<any> = await client.pg.pg.execute(
    pgDescriptor,
    params.text,
    params.values
  )

  return result
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

  public hasTimeseries(tsCtx: TimeseriesContext): boolean {
    const tsName = `${tsCtx.nodeType}$${tsCtx.field}`
    return !!this.tsCache.index[tsName]
  }

  public async execute<T>(
    tsCtx: string,
    query: string,
    params: unknown[]
  ): Promise<QueryResult<T>> {
    const [nodeType, field, shard] = tsCtx.split('$')

    const { descriptor } = this.getShards({ nodeType, field })[shard]
    return this.pg.execute(descriptor, query, params)
  }

  private getShards(tsCtx: TimeseriesContext): Shard[] {
    const tsName = `${tsCtx.nodeType}$${tsCtx.field}`
    const shards = this.tsCache.index[tsName]
    if (!shards || !shards[0]) {
      return []
    }

    let shardList = Object.keys(shards)
      .map((k) => Number(k))
      .sort((a, b) => a - b)
      .map((ts) => {
        return {
          ts,
          descriptor: shards[ts].descriptor,
          size: shards[ts].meta.size,
        }
      })

    return shardList
  }

  async ensureTableExists(
    context: TimeseriesContext,
    ts: number = 0
  ): Promise<void> {
    const tsName = `${context.nodeType}$${context.field}`
    const tableName = `${tsName}$${ts}`

    const createTable = `
    CREATE TABLE IF NOT EXISTS "${tableName}" (
      "nodeId" text,
      payload ${SELVA_TO_SQL_TYPE[context.fieldSchema.type]},
      ts TIMESTAMP,
      "fieldSchema" jsonb
    );
    `
    console.log(`running: ${createTable}`)
    const pg = this.client.pg.getMinInstance()
    await pg.execute<void>(createTable, [])

    const createNodeIdIndex = `CREATE INDEX IF NOT EXISTS "${tableName}_node_id_idx" ON "${tableName}" ("nodeId");`

    console.log(`running: ${createNodeIdIndex}`)
    await pg.execute<void>(createNodeIdIndex, [])

    const { meta: current } = this.client.pg.tsCache.instances[pg.id]
    const stats = {
      cpu: current.cpu,
      memory: current.memory,
      timestamp: Date.now(),
      tableMeta: {
        [tableName]: {
          tableName,
          tableSizeBytes: 0,
          relationSizeBytes: 0,
        },
      },
    }

    this.client.pg.tsCache.updateIndexByInstance(pg.id, { stats })
    this.client.redis.publish(
      { type: 'timeseriesRegistry' },
      constants.TS_REGISTRY_UPDATE,
      JSON.stringify({
        event: 'new_shard',
        ts: Date.now(),
        id: pg.id,
        data: { stats },
      })
    )
  }

  public async insert<T>(
    tsCtx: TimeseriesContext,
    entry: { nodeId: string; ts: number; payload: any }
  ): Promise<QueryResult<T>> {
    const shards = this.getShards(tsCtx)
    if (!shards.length) {
      await this.ensureTableExists(tsCtx)
      return this.insert(tsCtx, entry)
    }

    const shard = shards[shards.length - 1]
    const tableName = `${tsCtx.nodeType}$${tsCtx.field}$${shard.ts}`

    const { nodeId, ts, payload } = entry
    return this.pg.execute(
      shard.descriptor,
      `INSERT INTO "${tableName}" ("nodeId", payload, ts, "fieldSchema") VALUES ($1, $2, $3, $4)`,
      [nodeId, payload, new Date(ts), tsCtx.fieldSchema]
    )
  }

  private selectShards(tsCtx: TimeseriesContext): Shard[] {
    // if we have a startTime, start iterating from beginning to find the shard to start
    // if we have an endTime, keep iterating till you go over
    //
    // if we only have endTime then iterate from the end to find where to end

    let shards = this.getShards(tsCtx)

    if (tsCtx.startTime) {
      let startIdx = 0
      let endIdx = shards.length - 1
      for (let i = 1; i < shards.length; i++) {
        const shard = shards[i]

        if (shard.ts > tsCtx.startTime) {
          break
        }

        startIdx = i
      }

      if (tsCtx.endTime) {
        endIdx = startIdx
        for (let i = startIdx + 1; i++; i < shards.length) {
          const shard = shards[i]

          if (shard.ts > tsCtx.endTime) {
            break
          }

          endIdx = i
        }
      }

      shards = shards.slice(startIdx, endIdx + 1)
      // FIXME: ?
      // if (tsCtx.order === 'desc') {
      //   shards.reverse()
      // } else {
      //   // timestamp between two values makes sense in default ascending order
      //   tsCtx.order = 'asc'
      // }
    } else if (tsCtx.endTime) {
      let endIdx = shards.length - 1

      for (let i = shards.length - 1; i >= 0; i--) {
        const shard = shards[i]
        if (tsCtx.endTime > shard.ts) {
          break
        }

        endIdx = i
      }

      shards = shards.slice(0, endIdx + 1)
    }

    if (tsCtx.order === 'desc') {
      shards.reverse()
    }

    return shards
  }

  // TODO: the query here needs to be a higher level consruct than SQL, because we need to adjust query contents based on shard targeted
  public async select<T>(
    tsCtx: TimeseriesContext,
    op: GetOperationFind | GetOperationAggregate
  ): Promise<QueryResult<T>> {
    // order is important
    const where = toExpr(tsCtx, tsCtx.fieldSchema, op.filter)
    const shards = this.selectShards(tsCtx)

    // apply context defaults for limiting result set size
    if (!(tsCtx.startTime && tsCtx.endTime) && tsCtx.limit === -1) {
      tsCtx.limit = DEFAULT_QUERY_LIMIT
    }

    return runSelect<T>(tsCtx, shards, this.client, op, where)
  }
}
