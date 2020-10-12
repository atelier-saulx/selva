import { SelvaClient } from '../../'
import { GetOperationInherit, GetResult } from '../types'
import { getNestedSchema } from '../utils'
import { TYPE_CASTS } from './'

export default async function(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  db: string
): Promise<GetResult> {
  const schema = client.schemas[db]
  // TODO: lang for text fields (this goes in create op)
  const prefixes: string = op.types.reduce((acc, t) => {
    const p = client.schemas[db].types[t].prefix
    if (p) {
      acc += p
    }

    return acc
  }, '')

  if (op.item) {
    return {}
  }

  if (op.single) {
    const fs = getNestedSchema(
      schema,
      schema.types[op.types[0]].prefix,
      <string>op.sourceField
    )

    const res = await client.redis.selva_inherit(
      {
        name: db
      },
      '___selva_hierarchy',
      op.id,
      prefixes,
      <string>op.sourceField // TODO?
    )

    if (TYPE_CASTS[fs.type]) {
      return TYPE_CASTS[fs.type](res[0][1])
    }

    return res.length ? res[0][1] : null
  }

  const remapped: Record<string, string> = {}
  const fields = Object.keys(op.props).map(f => {
    if (typeof op.props[f] === 'string') {
      remapped[<string>op.props[f]] = f
      return <string>op.props[f]
    }

    return f
  })

  const res = await client.redis.selva_inherit(
    {
      name: db
    },
    '___selva_hierarchy',
    op.id,
    prefixes,
    ...fields
  )

  const o: GetResult = {}
  for (let i = 0; i < res.length; i++) {
    let [f, v] = res[i]
    const fs = getNestedSchema(schema, schema.types[op.types[0]].prefix, f)
    const typeCast = TYPE_CASTS[fs.type]

    if (remapped[f]) {
      f = remapped[f]
    }

    o[f.slice(op.field.length + 1)] = typeCast ? typeCast(v) : v
  }

  return o
}
