import { SelvaClient } from '../../'
import { GetOperationAggregate, GetOperationFind } from '../types'
import {
  executeNestedGetOperations,
  ExecContext,
  sourceFieldToDir,
  addMarker,
} from './'

export default async function execTimeseries(
  client: SelvaClient,
  op: GetOperationFind | GetOperationAggregate,
  lang: string,
  ctx: ExecContext
): Promise<any> {
  console.log('IS TIMESERIES', JSON.stringify(op, null, 2))
  return null
}
