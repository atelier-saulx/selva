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
  const o: GetResult = {}
  for (let i = 0; i < res.length; i++) {
    let [idx, f, v] = res[i]

    console.log('HMM', idx, f, v, res[i])
    const newV = typeCast(v, idx, f, client.schemas[ctx.db], lang)

    if (remapped[f]) {
      f = remapped[f]
    }

    setNestedResult(o, f.slice(field.length + 1), newV)
  }

  return o
}
