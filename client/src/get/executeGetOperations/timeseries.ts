import { QueryResult } from 'pg'
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
import {
  getNestedField,
  getNestedSchema,
  getTypeFromId,
  setNestedResult,
} from '../utils'
import {
  executeNestedGetOperations,
  ExecContext,
  sourceFieldToDir,
  addMarker,
  executeGetOperation,
} from './'
import { TimeseriesContext } from '../../timeseries'

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
  type: string,
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
          e.context.nodeType === type &&
          e.context.field === field &&
          (!exprCtx.startTime ? true : e.context.ts >= exprCtx.startTime) &&
          (!exprCtx.endTime ? true : e.context.ts <= exprCtx.endTime)
        )
      })
  } catch (_e) {
    return []
  }
}

export default async function execTimeseries(
  client: SelvaClient,
  op: GetOperationFind | GetOperationAggregate,
  lang: string,
  ctx: ExecContext
): Promise<any> {
  await client.pg.connect()

  const fieldSchema = getNestedSchema(
    client.schemas[ctx.db],
    op.id,
    <string>op.sourceField
  )

  if (!fieldSchema) {
    return
  }

  addMarker(client, ctx, {
    type: 'node',
    id: op.id,
    fields: [<string>op.sourceField],
  })

  if (ctx.subId && !ctx.firstEval) {
    console.log('NOT FIRST EVAL OF TIMESERIES, GETTING CURRENT VALUE')
    return executeGetOperation(
      client,
      lang,
      ctx,
      { type: 'db', id: op.id, sourceField: op.sourceField, field: op.field },
      true
    )
  }

  const type = getTypeFromId(client.schemas[ctx.db], op.id)

  const exprCtx: TimeseriesContext = {
    nodeType: type,
    field: <string>op.sourceField,
    order: op.options?.sort?.$order || 'asc',
  }

  let sql = sq
    .select({
      autoQuoteTableNames: true,
      autoQuoteAliasNames: true,
      nameQuoteCharacter: '"',
    })
    .from(`${type}$${op.sourceField}$0`)
    .field('ts')
    .where('"nodeId" = ?', op.id)
    .where(toExpr(exprCtx, fieldSchema, op.filter))
    .limit(op.options.limit === -1 ? null : op.options.limit)
    .offset(op.options.offset === 0 ? null : op.options.offset)

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

  const insertQueue = await getInsertQueue(
    client,
    exprCtx,
    type,
    op.id,
    <string>op.sourceField
  )

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
      .where(
        toExpr(
          { nodeType: type, field: <string>op.sourceField },
          fieldSchema,
          op.filter
        )
      )

    if (['object', 'record'].includes(fieldSchema.type)) {
      qSql = qSql.field('payload')
    } else {
      qSql = qSql.field('payload', 'value')
    }

    qSql = qSql.order(
      // TODO
      // isObj
      //   ? op.options?.sort?.$field || 'ts'
      //   : op.options?.sort?.$field
      //   ? 'payload'
      //   : 'ts',
      'ts',
      exprCtx.order === 'asc'
    )

    sql = sql.union_all(qSql)
  } else {
    sql = sql.order(
      // TODO
      // isObj
      //   ? op.options?.sort?.$field || 'ts'
      //   : op.options?.sort?.$field
      //   ? 'payload'
      //   : 'ts',
      'ts',
      exprCtx.order === 'asc'
    )
  }

  const params = sql.toParam({ numberedParametersStartAt: 1 })
  console.log('SQL', params, 'SELECTOR', exprCtx)
  // TODO: this needs to call select with op
  const result: QueryResult<any> = await client.pg.insert(
    // TODO: get startTime and endTime from filters
    {
      nodeType: type,
      field: <string>op.sourceField,
      startTime: exprCtx.startTime,
      endTime: exprCtx.endTime,
    },
    params.text,
    params.values
  )

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
