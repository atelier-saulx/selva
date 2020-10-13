import { SelvaClient } from '../../'
import { GetOperationInherit, GetResult } from '../types'
import { getNestedSchema, setNestedResult } from '../utils'
import { TYPE_CASTS } from './'

async function getObject(
  client: SelvaClient,
  db: string,
  field: string,
  id: string
): Promise<GetResult> {
  const res = await client.redis.hgetall({ name: db }, id)
  const o: GetResult = {}
  for (const k in res) {
    if (
      k.length > field.length &&
      k.startsWith(field) &&
      res[k] !== '___selva_$object'
    ) {
      const f = k.slice(field.length + 1)
      setNestedResult(o, f, res[k])
    }
  }

  return o
}

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

    const v = res.length ? res[0][2] : null

    if (v === '___selva_$object') {
      return await getObject(client, db, <string>op.sourceField, res[0][0])
    }

    if (TYPE_CASTS[fs.type]) {
      return TYPE_CASTS[fs.type](v)
    }

    return v
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
    let [idx, f, v] = res[i]
    const fs = getNestedSchema(schema, schema.types[op.types[0]].prefix, f)

    if (v === '___selva_$object') {
      return await getObject(client, db, <string>op.sourceField, idx)
    }

    const typeCast = TYPE_CASTS[fs.type]

    if (remapped[f]) {
      f = remapped[f]
    }

    o[f.slice(op.field.length + 1)] = typeCast ? typeCast(v) : v
  }

  return o
}
