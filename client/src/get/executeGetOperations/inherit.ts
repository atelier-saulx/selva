import { SelvaClient } from '../../'
import {
  GetOperationInherit,
  GetResult,
  Fork,
  GetOptions,
  FilterAST,
} from '../types'
import { getNestedSchema, setNestedResult } from '../utils'
import {
  TYPE_CASTS,
  typeCast,
  ExecContext,
  executeNestedGetOperations,
  addMarker,
  bufferNodeMarker,
} from './'
import { Schema, FieldSchema } from '../../schema'
import { ast2rpn } from '@saulx/selva-query-ast-parser'
import { buildResultFromIdFieldAndValue, makeLangArg } from './util'

function makeRealKeys(
  props: GetOptions,
  field: string,
  simple?: boolean
): [Record<string, true | string>, Record<string, any>] {
  const defaults: Record<string, any> = {}

  if (simple) {
    return [props, null]
  }

  const p = field + '.'

  const realKeys: Record<string, true | string> = {}

  for (const prop in props) {
    if (!prop.startsWith('$')) {
      if (props[prop].$field) {
        realKeys[p + prop] = <string>props[prop].$field
      } else if (props[prop].$default !== undefined) {
        realKeys[p + prop] = true
        defaults[p + prop] = props[prop].$default
      } else {
        realKeys[p + prop] = true
      }
    }
  }

  return [realKeys, defaults]
}

async function mergeObj(
  client: SelvaClient,
  op: GetOperationInherit,
  _schema: Schema,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  const remapped: Record<string, string> = {}
  const [props, defaults] = makeRealKeys(op.props, op.field, true)
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

  if (op.types && op.types.length) {
    const $or: FilterAST[] = op.types.map((t) => {
      return {
        $operator: '=',
        $field: 'type',
        $value: t,
      }
    })

    $or.push({
      $operator: '=',
      $field: 'id',
      $value: op.id,
    })

    fork.$and.push({
      isFork: true,
      $or,
    })
  }

  const rpn = ast2rpn(client.schemas[ctx.db].types, fork)

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
    makeLangArg(client.schemas[ctx.db].languages, lang),
    '___selva_hierarchy',
    'ancestors',
    'offset',
    -1,
    'merge',
    field,
    op.id,
    ...rpn
  )

  const o = buildResultFromIdFieldAndValue(
    ctx,
    client,
    remapped,
    field,
    res,
    defaults,
    lang
  )

  return o
}

async function deepMergeObj(
  client: SelvaClient,
  op: GetOperationInherit,
  _schema: Schema,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  const remapped: Record<string, string> = {}
  const [props, defaults] = makeRealKeys(op.props, op.field, true)
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

  if (op.types && op.types.length) {
    const $or: FilterAST[] = op.types.map((t) => {
      return {
        $operator: '=',
        $field: 'type',
        $value: t,
      }
    })

    $or.push({
      $operator: '=',
      $field: 'id',
      $value: op.id,
    })

    fork.$and.push({
      isFork: true,
      $or,
    })
  }

  const rpn = ast2rpn(client.schemas[ctx.db].types, fork)

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
    makeLangArg(client.schemas[ctx.db].languages, lang),
    '___selva_hierarchy',
    'ancestors',
    'offset',
    -1,
    'deepMerge',
    field,
    op.id,
    ...rpn
  )

  const o = buildResultFromIdFieldAndValue(
    ctx,
    client,
    remapped,
    field,
    res,
    defaults
  )
  return o
}

async function inheritItem(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> {
  const schema = client.schemas[ctx.db]

  const [props] = makeRealKeys(op.props, op.field)
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

  const rpn = ast2rpn(client.schemas[ctx.db].types, fork)

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
    makeLangArg(client.schemas[ctx.db].languages, lang),
    '___selva_hierarchy',
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
    // TODO: needs $default?
  }

  return entryRes
}

export default async function inherit(
  client: SelvaClient,
  op: GetOperationInherit,
  lang: string,
  ctx: ExecContext,
  passedOnSchema?: Schema
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
  const schema = passedOnSchema || client.schemas[db]
  if (op.item) {
    return inheritItem(client, op, lang, ctx)
  }

  const prefixes: string = op.types.reduce((acc, t) => {
    if (t === 'root') {
      return acc + 'ro'
    }

    const p = (passedOnSchema || client.schemas[db]).types[t].prefix
    if (p) {
      acc += p
    }

    return acc
  }, '')

  let fs: FieldSchema | null
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
      const added = await addMarker(
        client,
        ctx,
        {
          type: 'ancestors',
          id: op.id,
          fields: [op.sourceField],
          rpn: ast2rpn(
            (passedOnSchema || client.schemas[ctx.db]).types,
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
        },
        passedOnSchema
      )

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
      makeLangArg((passedOnSchema || client.schemas[ctx.db]).languages, lang),
      '___selva_hierarchy',
      op.id,
      prefixes,
      <string>op.sourceField // TODO?
    )

    const v = res.length ? res[0][2] : null

    if (!v) {
      return null
    }

    const p = Object.assign({}, op.props, { $id: v })
    delete p.$inherit
    delete p.$field

    return executeNestedGetOperations(
      client,
      p,
      lang,
      ctx,
      false,
      passedOnSchema
    )
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
      const added = await addMarker(
        client,
        ctx,
        {
          type: 'ancestors',
          id: op.id,
          fields: [op.sourceField],
          rpn: ast2rpn(
            (passedOnSchema || client.schemas[ctx.db]).types,
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
        },
        passedOnSchema
      )

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
      makeLangArg((passedOnSchema || client.schemas[ctx.db]).languages, lang),
      '___selva_hierarchy',
      op.id,
      prefixes,
      <string>op.sourceField // TODO?
    )

    const v = res.length ? res[0][2] : null

    if (v === null) {
      return null
    }

    if (TYPE_CASTS[fs.type]) {
      const field = res[0][1]
      return TYPE_CASTS[fs.type](
        v,
        op.id,
        field,
        passedOnSchema || client.schemas[ctx.db],
        lang
      )
    }

    return v
  }

  const [realKeys, defaults] = makeRealKeys(op.props, op.field)
  const remapped: Record<string, string> = {}
  const fields = Object.keys(realKeys).map((f) => {
    if (typeof realKeys[f] === 'string') {
      remapped[<string>realKeys[f]] = f
      return <string>realKeys[f]
    }
    return f
  })

  if (fields.length === 0) {
    if (op.props.$all) {
      fields.push(`${op.sourceField}.*`)
    } else {
      fields.push(op.sourceField)
    }
  }

  if (ctx.subId) {
    const sourceFieldSchema = getNestedSchema(schema, op.id, op.sourceField)

    let added: boolean
    if (sourceFieldSchema && sourceFieldSchema.type === 'reference') {
      bufferNodeMarker(ctx, op.id, op.sourceField)
      added = await addMarker(
        client,
        ctx,
        {
          type: 'bfs_expression',
          id: op.id,
          traversal: `{"parents","${op.sourceField}"}`,
          fields: fields.map((f: string) =>
            f.substring(op.sourceField.length + 1)
          ),
          rpn: ast2rpn(
            client.schemas[ctx.db].types,
            {
              isFork: true,
              $and: [
                {
                  isFork: true,
                  $or: fields.map((f) => {
                    return {
                      $operator: 'exists',
                      $field: f.substring(op.sourceField.length + 1),
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
        },
        passedOnSchema
      )
    } else {
      bufferNodeMarker(ctx, op.id, ...fields)
      added = await addMarker(
        client,
        ctx,
        {
          type: 'ancestors',
          id: op.id,
          fields,
          rpn: ast2rpn(
            client.schemas[ctx.db].types,
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
        },
        passedOnSchema
      )
    }

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
    makeLangArg(client.schemas[ctx.db].languages, lang),
    '___selva_hierarchy',
    op.id,
    prefixes,
    ...fields
  )

  const propsLen = Object.keys(op.props).length
  if (res.length === 0) {
    return null
  } else if (propsLen === 1 || (propsLen === 2 && op.props.$field)) {
    const [idx, f, v] = res[0]
    const fs = getNestedSchema(schema, idx, f)
    const typeCast = TYPE_CASTS[fs.type]
    return typeCast ? typeCast(v, idx, f, client.schemas.default, lang) : v
  } else if (op.props.$all) {
    const [idx, f, v] = res[0]
    const fs = getNestedSchema(schema, op.id, f)

    if (fs && ['reference', 'references'].includes(fs.type)) {
      const obj = {}
      for (let i = 0; i < v.length; i += 2) {
        const f1 = v[i]
        const v1 = v[i + 1]
        const fs1 = getNestedSchema(schema, idx, f1)
        const typeCast = TYPE_CASTS[fs1.type]
        const v2 = typeCast
          ? typeCast(v1, idx, f1, client.schemas.default, lang)
          : v1

        setNestedResult(obj, f1, v2)
      }

      return obj
    } else {
      const typeCast = TYPE_CASTS[fs.type]
      return typeCast ? typeCast(v, idx, f, client.schemas.default, lang) : v
    }
  }

  const o = buildResultFromIdFieldAndValue(
    ctx,
    client,
    remapped,
    op.field,
    res,
    defaults
  )
  return o
}
