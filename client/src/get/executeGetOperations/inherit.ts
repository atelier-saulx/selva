import { SelvaClient } from '../../'
import { GetOperationInherit, GetResult } from '../types'

export default async function(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  db: string
): Promise<GetResult> {
  // TODO: lang for text fields (this goes in create op)
  const prefixes: string = op.types.reduce((acc, t) => {
    const p = client.schemas[db].types[t].prefix
    if (p) {
      acc += p
    }

    return acc
  }, '')

  return client.redis.selva_inherit(
    {
      name: db
    },
    '___selva_hierarchy',
    op.id,
    prefixes,
    ...Object.keys(op.props)
  )
}
