import { SelvaClient } from '../../'
import { GetOperationAggregate, GetOperationFind } from '../types'
import { getNestedSchema, getTypeFromId } from '../utils'
import { ExecContext, addMarker, executeGetOperation } from './'
import { TimeseriesContext } from '../../timeseries'

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
    fieldSchema,
    order: op.options?.sort?.$order,
    limit: op.options.limit || -1,
    offset: op.options.offset || 0,
  }

  return client.pg.select(exprCtx, op)
}
