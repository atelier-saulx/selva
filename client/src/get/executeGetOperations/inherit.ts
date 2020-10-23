import { SelvaClient } from '../../'
import { GetOperationInherit, GetResult, GetOperation, Fork } from '../types'
import { getNestedSchema, getNestedField } from '../utils'
import executeGetOperations, { TYPE_CASTS, ExecContext } from './'
import { FieldSchema } from '../../schema'
import { ast2rpn } from '@saulx/selva-query-ast-parser'

async function mergeObj(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  const { db } = ctx
  const remapped: Record<string, string> = {}
  const fields = Object.keys(op.props).map(f => {
    if (typeof op.props[f] === 'string') {
      remapped[<string>op.props[f]] = f
      return <string>op.props[f]
    }

    return f
  })

  const field = fields[0]

  const fork: Fork = {
    isFork: true,
    $and: [
      {
        $operator: 'exists',
        $field: field
      },
      {
        isFork: true,
        $or: op.types.map(t => {
          return {
            $operator: '=',
            $field: 'type',
            $value: t
          }
        })
      }
    ]
  }

  const rpn = ast2rpn(fork)

  const ids = await client.redis.selva_hierarchy_find(
    {
      name: db
    },
    '___selva_hierarchy',
    'bfs',
    'ancestors',
    op.id,
    ...rpn
  )
  console.log('___selva_hierarchy', 'bfs', 'ancestors', op.id, rpn);
  console.log(ids)

  if (!ids || !ids.length) {
    return null
  }

  ids.unshift(op.id)

  const objs: GetResult[] = (
    await Promise.all(
      ids.map(async idx => {
        const o = await executeGetOperations(client, lang, ctx, [
          {
            id: idx,
            type: 'db',
            field,
            sourceField: field
          }
        ])

        return getNestedField(o, field)
      })
    )
  ).filter(x => !!x)

  const o: GetResult = {}
  for (const obj of objs) {
    for (const k in obj) {
      if (o[k]) {
        continue
      }

      o[k] = obj[k]
    }
  }

  return o
}

async function inheritItem(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  const { db } = ctx

  const remapped: Record<string, string> = {}
  const fields = Object.keys(op.props).map(f => {
    f = f.slice(op.field.length + 1)
    if (typeof op.props[f] === 'string') {
      remapped[<string>op.props[f]] = f
      return <string>op.props[f]
    }

    return f
  })

  let fork: Fork = {
    isFork: true,
    $or: op.types.map(t => {
      return {
        $operator: '=',
        $field: 'type',
        $value: t
      }
    })
  }

  if (op.required) {
    fork = {
      isFork: true,
      $and: [
        {
          isFork: true,
          $and: op.required.map(f => {
            return {
              $operator: 'exists',
              $field: f
            }
          })
        },
        fork
      ]
    }
  }

  const rpn = ast2rpn(fork)

  const [id] = await client.redis.selva_hierarchy_find(
    {
      name: db
    },
    '___selva_hierarchy',
    'bfs',
    'ancestors',
    'limit',
    1,
    op.id,
    ...rpn
  )

  if (!id) {
    return null
  }

  const ops: GetOperation[] = fields.map(f => {
    return {
      id,
      type: 'db',
      field: f,
      sourceField: f
    }
  })

  const o = await executeGetOperations(client, lang, ctx, ops)
  return o
}

async function getObject(
  client: SelvaClient,
  ctx: ExecContext,
  field: string,
  _fieldSchema: FieldSchema,
  lang: string | undefined,
  id: string
): Promise<GetResult> {
  const op: GetOperation = {
    type: 'db',
    id,
    field: field,
    sourceField: field
  }

  const o = await executeGetOperations(client, lang, ctx, [op])
  return getNestedField(o, field)
}

export default async function inherit(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  if (Array.isArray(op.sourceField)) {
    for (const sf of op.sourceField) {
      const r = await inherit(
        client,
        Object.assign({}, op, { sourceField: sf }),
        lang,
        ctx
      )

      if (r) {
        return r
      }
    }

    return
  }

  const { db } = ctx
  const schema = client.schemas[db]
  if (op.item) {
    return inheritItem(client, op, lang, ctx)
  }

  const prefixes: string = op.types.reduce((acc, t) => {
    if (t === 'root') {
      return acc + 'ro'
    }

    const p = client.schemas[db].types[t].prefix
    if (p) {
      acc += p
    }

    return acc
  }, '')

  if (op.single) {
    const fs = getNestedSchema(
      schema,
      schema.types[op.types[0]].prefix,
      <string>op.sourceField
    )

    if (op.merge === true && (fs.type === 'object' || fs.type === 'record')) {
      return mergeObj(client, op, lang, ctx)
    }

    const res = await client.redis.selva_inherit(
      {
        name: db
      },
      '___selva_hierarchy',
      op.id,
      prefixes,
      <string>op.sourceField // TODO?
    )
    let v = res.length ? res[0][2] : null

    if (TYPE_CASTS[fs.type]) {
      return TYPE_CASTS[fs.type](v)
    } if (fs.type === 'text') {
      const result = {};

      for (let i = 0; i < v.length; i += 2) {
          result[v[i]] = v[i + 1]
      }

      if  (lang) {
        v = result[lang] || null

        if (!v && client.schemas.default.languages) {
          for (const l of client.schemas.default.languages) {
            const txt = result[l]
            if (txt)
              return txt
          }
        }

        return v
      }

      return result
    } else if (fs.type === 'object') {
      const [id, field, value] = res[0]
      const result: any = {}
      // This is a lousy copy from executeGetOperations/index.ts
      const parse = (o, field: string, arr: string[]) => Promise.all(arr.map(async (key, i, arr) => {
        if ((i & 1) === 1) return
        let val = arr[i + 1]

        if (val === '___selva_$set') {
          const set = await client.redis.zrange(id + '.' + key, 0, -1)
          if (set) {
            o[key] = set
          }
        } else if (Array.isArray(val)) {
          o[key] = {}
          await parse(o[key], `${field}.${key}`, val)
        } else {
          const fieldSchema = getNestedSchema(client.schemas.default, id, `${field}.${key}`)
          const typeCast = TYPE_CASTS[fieldSchema.type]
          if (typeCast) {
            val = typeCast(val)
          }
          o[key] = val
        }
      }))

      await parse(result, field, value)

      return result
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
      return await getObject(client, ctx, <string>op.sourceField, fs, lang, idx)
    }

    const typeCast = TYPE_CASTS[fs.type]

    if (remapped[f]) {
      f = remapped[f]
    }

    o[f.slice(op.field.length + 1)] = typeCast ? typeCast(v) : v
  }

  return o
}
