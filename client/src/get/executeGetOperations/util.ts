import { SelvaClient } from '../../'
import { GetResult } from '../types'
import { setNestedResult } from '../utils'
import { ExecContext, typeCast } from './'

export function buildResultFromIdFieldAndValue(
  ctx: ExecContext,
  client: SelvaClient,
  remapped: Record<string, string>,
  field: string,
  res: string[],
  lang?: string
): GetResult {
  console.log('HMM RES', res)
  const o: GetResult = {}
  for (let i = 0; i < res.length; i++) {
    let [idx, f, v] = res[i]
    console.log('HMM DOING', idx, f, v)

    const newV = typeCast(v, idx, f, client.schemas[ctx.db], lang)

    if (remapped[f]) {
      f = remapped[f]
    }

    setNestedResult(o, f, newV)
  }

  console.log('O', o)
  return o
}
