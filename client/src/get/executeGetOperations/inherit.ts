import { SelvaClient } from '../../'
import { GetOperationInherit, GetResult } from '../types'

export default async function(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  db: string
): Promise<GetResult> {
  // TODO: lang for text fields (this goes in create op)
  return client.redis.selva_inherit(
    {
      name: db
    },
    '___selva_hierarchy',
    op.id,
    ...Object.keys(op.props)
  )
}
