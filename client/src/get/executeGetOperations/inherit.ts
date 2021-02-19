import { SelvaClient } from '../../'
import {
  GetOperationInherit,
  GetResult,
  GetOperation,
  Fork,
  GetOptions,
} from '../types'
import { getNestedSchema, getNestedField, setNestedResult } from '../utils'
import executeGetOperations, {
  TYPE_CASTS,
  typeCast,
  ExecContext,
  executeNestedGetOperations,
  addMarker,
  bufferNodeMarker,
  executeGetOperation,
} from './'
import { FieldSchema, Schema } from '../../schema'
import { ast2rpn } from '@saulx/selva-query-ast-parser'
import { deepMerge } from '@saulx/utils'
import { buildResultFromIdFieldAndValue } from './util'

function makeRealKeys(
  props: GetOptions,
  field: string,
  simple?: boolean
): Record<string, true | string> {
  if (simple) {
    return props
  }

  const p = field + '.'

  let realKeys: Record<string, true | string> = {}

  for (const prop in props) {
    if (!prop.startsWith('$')) {
      if (props[prop].$field) {
        realKeys[p + prop] = <string>props[prop].$field
      } else {
        realKeys[p + prop] = true
      }
    }
  }

  return realKeys
}

async function mergeObj(
  client: SelvaClient,
  op: GetOperationInherit,
  schema: Schema,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  const { db } = ctx
  const remapped: Record<string, string> = {}
  const props = makeRealKeys(op.props, op.field, true)
  const fields = Object.keys(props).map((f) => {
    if (typeof props[f] === 'string') {
      remapped[<string>props[f]] = f
      return <string>props[f]
    }

    return f
  })

  const field = fields[0]

  const fork: Fork = {
    isFork: true,
    $and: [
      {
        $operator: 'exists',
        $field: field,
      },
    ],
  }

  if (op.types) {
    fork.$and.push({
      isFork: true,
      $or: op.types.map((t) => {
        return {
          $operator: '=',
          $field: 'type',
          $value: t,
        }
      }),
    })
  }

  const rpn = ast2rpn(fork)

  if (ctx.subId) {
    bufferNodeMarker(ctx, op.id, ...fields)
    const added = await addMarker(client, ctx, {
      type: 'ancestors',
      id: op.id,
      fields,
      rpn,
    })

    if (added) {
      client.redis.selva_subscriptions_refresh(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '___selva_hierarchy',
        ctx.subId
      )
      ctx.hasFindMarkers = true
    }
  }

  const res = await client.redis.selva_hierarchy_find(
    ctx.originDescriptors[ctx.db] || { name: ctx.db },
    '___selva_hierarchy',
    'bfs',
    'ancestors',
    'offset',
    -1,
    'merge',
    field,
    op.id,
    ...rpn
  )
  console.log('RESSS', res)
  console.log(
    '___selva_hierarchy',
    'bfs',
    'offset',
    -1,
    'ancestors',
    'merge',
    field,
    op.id,
    rpn
  )

  const o = buildResultFromIdFieldAndValue(
    ctx,
    client,
    remapped,
    field,
    res,
    lang
  )

  return o
}

async function deepMergeObj(
  client: SelvaClient,
  op: GetOperationInherit,
  schema: Schema,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  const { db } = ctx
  const remapped: Record<string, string> = {}
  const props = makeRealKeys(op.props, op.field, true)
  const fields = Object.keys(props).map((f) => {
    if (typeof props[f] === 'string') {
      remapped[<string>props[f]] = f
      return <string>props[f]
    }

    return f
  })

  const field = fields[0]

  const fork: Fork = {
    isFork: true,
    $and: [
      {
        $operator: 'exists',
        $field: field,
      },
    ],
  }

  if (op.types) {
    fork.$and.push({
      isFork: true,
      $or: op.types.map((t) => {
        return {
          $operator: '=',
          $field: 'type',
          $value: t,
        }
      }),
    })
  }

  const rpn = ast2rpn(fork)

  if (ctx.subId) {
    bufferNodeMarker(ctx, op.id, ...fields)
    const added = await addMarker(client, ctx, {
      type: 'ancestors',
      id: op.id,
      fields,
      rpn,
    })

    if (added) {
      client.redis.selva_subscriptions_refresh(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '___selva_hierarchy',
        ctx.subId
      )
      ctx.hasFindMarkers = true
    }
  }

  const res = await client.redis.selva_hierarchy_find(
    ctx.originDescriptors[ctx.db] || { name: ctx.db },
    '___selva_hierarchy',
    'bfs',
    'ancestors',
    'offset',
    -1,
    'deepMerge',
    field,
    op.id,
    ...rpn
  )
  console.log(
    '___selva_hierarchy',
    'bfs',
    'ancestors',
    'offset',
    -1,
    'deepMerge',
    field,
    op.id,
    rpn
  )

  const o = buildResultFromIdFieldAndValue(ctx, client, remapped, field, res)
  return o
}

async function inheritItem(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  const { db } = ctx
  const schema = client.schemas[ctx.db]

  const props = makeRealKeys(op.props, op.field)
  const remapped: Record<string, string> = {}
  const fields = Object.keys(props).map((f) => {
    f = f.slice(op.field.length + 1)
    if (typeof props[f] === 'string') {
      remapped[<string>props[f]] = f
      return <string>props[f]
    }

    return f
  })

  let fork: Fork = {
    isFork: true,
    $or: op.types.map((t) => {
      return {
        $operator: '=',
        $field: 'type',
        $value: t,
      }
    }),
  }

  if (op.required) {
    fork = {
      isFork: true,
      $and: [
        {
          isFork: true,
          $and: op.required.map((f) => {
            return {
              $operator: 'exists',
              $field: f,
            }
          }),
        },
        fork,
      ],
    }
  }

  const rpn = ast2rpn(fork)

  if (ctx.subId) {
    bufferNodeMarker(ctx, op.id, ...fields)
    const added = await addMarker(client, ctx, {
      type: 'ancestors',
      id: op.id,
      fields,
      rpn,
    })

    if (added) {
      client.redis.selva_subscriptions_refresh(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '___selva_hierarchy',
        ctx.subId
      )
      ctx.hasFindMarkers = true
    }
  }

  const [results] = await client.redis.selva_hierarchy_find(
    ctx.originDescriptors[ctx.db] || { name: ctx.db },
    '___selva_hierarchy',
    'bfs',
    'ancestors',
    'limit',
    1,
    'fields',
    fields ? fields.join('\n') : '',
    op.id,
    ...rpn
  )

  if (!results) {
    return null
  }

  const [id, fieldResults] = results

  if (!id) {
    return null
  }

  const entryRes: any = {}
  for (let i = 0; i < fieldResults.length; i += 2) {
    const field = fieldResults[i]
    const value = fieldResults[i + 1]

    if (field === 'id') {
      entryRes.id = id
      continue
    }

    setNestedResult(entryRes, field, typeCast(value, id, field, schema, lang))
  }

  return entryRes
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

  let fs
  if (op.types && op.types.length > 0) {
    fs = getNestedSchema(
      schema,
      (op.types[0] === 'root' ? schema.rootType : schema.types[op.types[0]])
        .prefix,
      <string>op.sourceField
    )
  }

  if (fs && fs.type === 'reference') {
    if (ctx.subId) {
      bufferNodeMarker(ctx, op.id, op.sourceField)
      const added = await addMarker(client, ctx, {
        type: 'ancestors',
        id: op.id,
        fields: [op.sourceField],
        rpn: ast2rpn(
          {
            isFork: true,
            $and: [
              {
                $operator: 'exists',
                $field: op.sourceField,
              },
              {
                isFork: true,
                $or: op.types.map((t) => {
                  return {
                    $operator: '=',
                    $field: 'type',
                    $value: t,
                  }
                }),
              },
            ],
          },
          lang
        ),
      })

      if (added) {
        client.redis.selva_subscriptions_refresh(
          ctx.originDescriptors[ctx.db] || { name: ctx.db },
          '___selva_hierarchy',
          ctx.subId
        )
        ctx.hasFindMarkers = true
      }
    }

    const res = await client.redis.selva_inherit(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      '___selva_hierarchy',
      op.id,
      prefixes,
      <string>op.sourceField // TODO?
    )
    let v = res.length ? res[0][2] : null

    if (!v) {
      return null
    }

    const p = Object.assign({}, op.props, { $id: v })
    delete p.$inherit
    delete p.$field

    return executeNestedGetOperations(client, p, lang, ctx)
  } else if (op.single) {
    if (
      op.merge === true &&
      (!fs || fs.type === 'object' || fs.type === 'record')
    ) {
      return mergeObj(client, op, schema, lang, ctx)
    }

    if (
      op.deepMerge === true &&
      (!fs || fs.type === 'object' || fs.type === 'record')
    ) {
      return deepMergeObj(client, op, schema, lang, ctx)
    }

    if (ctx.subId) {
      bufferNodeMarker(ctx, op.id, op.sourceField)
      const added = await addMarker(client, ctx, {
        type: 'ancestors',
        id: op.id,
        fields: [op.sourceField],
        rpn: ast2rpn(
          {
            isFork: true,
            $and: [
              {
                $operator: 'exists',
                $field: op.sourceField,
              },
              {
                isFork: true,
                $or: op.types.map((t) => {
                  return {
                    $operator: '=',
                    $field: 'type',
                    $value: t,
                  }
                }),
              },
            ],
          },
          lang
        ),
      })

      if (added) {
        client.redis.selva_subscriptions_refresh(
          ctx.originDescriptors[ctx.db] || { name: ctx.db },
          '___selva_hierarchy',
          ctx.subId
        )
        ctx.hasFindMarkers = true
      }
    }

    const res = await client.redis.selva_inherit(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      '___selva_hierarchy',
      op.id,
      prefixes,
      <string>op.sourceField // TODO?
    )
    let v = res.length ? res[0][2] : null

    if (v === null) {
      return null
    }

    if (TYPE_CASTS[fs.type]) {
      const field = res[0][1]
      return TYPE_CASTS[fs.type](v, op.id, field, client.schemas[ctx.db], lang)
    }

    return v
  }

  const realKeys = makeRealKeys(op.props, op.field)
  const remapped: Record<string, string> = {}
  const fields = Object.keys(realKeys).map((f) => {
    if (typeof realKeys[f] === 'string') {
      remapped[<string>realKeys[f]] = f
      return <string>realKeys[f]
    }

    return f
  })

  if (fields.length === 0) {
    fields.push(op.sourceField)
  }

  if (ctx.subId) {
    bufferNodeMarker(ctx, op.id, ...fields)
    const added = await addMarker(client, ctx, {
      type: 'ancestors',
      id: op.id,
      fields,
      rpn: ast2rpn(
        {
          isFork: true,
          $and: [
            {
              isFork: true,
              $or: fields.map((f) => {
                return {
                  $operator: 'exists',
                  $field: f,
                }
              }),
            },
            {
              isFork: true,
              $or: op.types.map((t) => {
                return {
                  $operator: '=',
                  $field: 'type',
                  $value: t,
                }
              }),
            },
          ],
        },
        lang
      ),
    })

    if (added) {
      client.redis.selva_subscriptions_refresh(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '___selva_hierarchy',
        ctx.subId
      )
      ctx.hasFindMarkers = true
    }
  }

  console.log(['___selva_hierarchy', op.id, prefixes || '', fields])
  let res = await client.redis.selva_inherit(
    ctx.originDescriptors[ctx.db] || { name: ctx.db },
    '___selva_hierarchy',
    op.id,
    prefixes,
    ...fields
  )

  if (res.length === 0) {
    return null
  } else if (Object.keys(op.props).length === 0) {
    let [idx, f, v] = res[0]
    const fs = getNestedSchema(schema, idx, f)
    const typeCast = TYPE_CASTS[fs.type]

    return typeCast ? typeCast(v, idx, f, client.schemas.default, lang) : v
  }

  const o = buildResultFromIdFieldAndValue(ctx, client, remapped, op.field, res)
  return o
}
