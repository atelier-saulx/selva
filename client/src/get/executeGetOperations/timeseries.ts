import { SelvaClient } from '../../'
import { GetOperationAggregate, GetOperationFind, GetOptions } from '../types'
import {
  getNestedField,
  getNestedSchema,
  getTypeFromId,
  setNestedResult,
} from '../utils'
import { ExecContext, addMarker, executeGetOperation } from './'
import { TimeseriesContext } from '../../timeseries'
import { CreatePartialDiff } from '@saulx/diff'

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

export default async function execTimeseries(
  client: SelvaClient,
  op: GetOperationFind | GetOperationAggregate,
  lang: string,
  ctx: ExecContext
): Promise<any> {
  await client.pg.connect()
  console.log(
    'MAKIN TIMESERIES',
    JSON.stringify(client.pg.tsCache.index, null, 2)
  )

  const fieldSchema = getNestedSchema(
    client.schemas[ctx.db],
    op.id,
    <string>op.sourceField
  )

  if (!fieldSchema) {
    return
  }

  const type = getTypeFromId(client.schemas[ctx.db], op.id)
  const exprCtx: TimeseriesContext = {
    nodeType: type,
    field: <string>op.sourceField,
    fieldSchema,
    order: op.options?.sort?.$order || 'desc',
    limit: op.options.limit || -1,
    offset: op.options.offset || 0,
  }

  addMarker(client, ctx, {
    type: 'node',
    id: op.id,
    fields: [exprCtx.field],
  })

  if (ctx.firstEval === false) {
    // TODO: here add diff function
    const [val, _ts] = await Promise.all([
      executeGetOperation(
        client,
        lang,
        ctx,
        { type: 'db', id: op.id, sourceField: op.sourceField, field: op.field },
        true
      ),
      executeGetOperation(
        client,
        lang,
        ctx,
        {
          type: 'raw',
          id: op.id,
          sourceField: `${op.sourceField}._ts`,
          field: 'ts',
        },
        true
      ),
    ])
    console.log('NOT FIRST EVAL OF TIMESERIES, GETTING CURRENT VALUE', _ts, val)
    return val
  }

  const fields: Set<string> = new Set()
  if (['object', 'record'].includes(fieldSchema.type)) {
    getFields('', fields, op.props)
    exprCtx.selectFields = fields
  }

  const result: any = await client.pg.select(exprCtx, op)
  return result.rows.map((row) => {
    if (fields.size) {
      const r: any = { value: {} }
      for (const f in row) {
        if (f === '_ts') {
          r.ts = new Date(row._ts).getTime()
        } else {
          r.value[f] = row[f]
        }
      }

      return r
    }

    return { ts: new Date(row._ts).getTime(), value: row.value }
  })
}
