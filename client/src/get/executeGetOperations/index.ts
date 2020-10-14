import { SelvaClient } from '../../'
import { GetOperation, GetResult } from '../types'
import { setNestedResult, getNestedSchema } from '../utils'
import resolveId from '../resolveId'
import createGetOperations from '../createGetOperations'
import { GetOptions } from '../'
import find from './find'
import inherit from './inherit'
import { Rpn } from '@saulx/selva-query-ast-parser'

export type ExecContext = {
  db: string
  subId?: string
  nodeMarkers?: Record<string, Set<string>>
}
export type SubscriptionMarker = {
  type: string // ancestors|descendants|<any other field traversed>
  id: string
  fields: string[]
  rpn?: Rpn
}

function adler32(marker: SubscriptionMarker): number {
  const MOD_ADLER = 65521

  const str: string = JSON.stringify(marker)

  let a = 1
  let b = 0
  for (let i = 0; i < str.length; i++) {
    a = (a + str.charCodeAt(i)) % MOD_ADLER
    b = (b + a) % MOD_ADLER
  }

  return (b << 16) | a
}

export async function addMarker(
  client: SelvaClient,
  ctx: ExecContext,
  marker: SubscriptionMarker
): Promise<void> {
  const markerId = adler32(marker)
  return client.redis.selva_subscriptions_add(
    { name: ctx.db },
    '___selva_hierarchy',
    ctx.subId,
    markerId,
    marker.type,
    marker.id,
    'fields',
    marker.fields.join('\n'),
    ...(marker.rpn ? ['filter', ...marker.rpn] : [])
  )
}

export function bufferNodeMarker(
  ctx: ExecContext,
  id: string,
  ...fields: string[]
): void {
  if (!ctx.subId) {
    return
  }

  if (!ctx.nodeMarkers) {
    ctx.nodeMarkers = {}
  }

  let current = ctx.nodeMarkers[id]
  if (!current) {
    current = new Set()
  }

  fields.forEach(f => current.add(f))

  ctx.nodeMarkers[id] = current
}

async function addNodeMarkers(
  client: SelvaClient,
  ctx: ExecContext
): Promise<void> {
  if (!ctx.nodeMarkers) {
    return
  }

  let count = 0
  try {
    await Promise.all(
      Object.entries(ctx.nodeMarkers).map(async ([id, fields]) => {
        count++
        console.log('adding marker', {
          type: 'node',
          id,
          fields: [...fields.values()]
        })

        return addMarker(client, ctx, {
          type: 'node',
          id,
          fields: [...fields.values()]
        })
      })
    )

    if (count > 0) {
      await client.redis.selva_subscriptions_refresh(
        { name: ctx.db },
        '___selva_hierarchy',
        ctx.subId
      )
    }
  } catch (e) {
    console.error(e)
  }
}

export const TYPE_CASTS: Record<string, (x: any) => any> = {
  float: Number,
  number: Number,
  int: Number,
  boolean: (x: any) => (x === '0' ? false : true),
  json: (x: any) => JSON.parse(x),
  array: (x: any) => JSON.parse(x)
}

const TYPE_TO_SPECIAL_OP: Record<
  string,
  (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => Promise<any>
> = {
  id: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => {
    return id
  },
  references: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => {
    const paddedId = id.padEnd(10, '\0')

    if (field === 'ancestors') {
      return client.redis.selva_hierarchy_find(
        '___selva_hierarchy',
        'bfs',
        'ancestors',
        paddedId
      )
    } else if (field === 'descendants') {
      return client.redis.selva_hierarchy_find(
        '___selva_hierarchy',
        'bfs',
        'descendants',
        paddedId
      )
    } else if (field === 'parents') {
      return client.redis.selva_hierarchy_parents('___selva_hierarchy', id)
    } else if (field === 'children') {
      return client.redis.selva_hierarchy_children('___selva_hierarchy', id)
    } else {
      return client.redis.zrange(id + '.' + field, 0, -1)
    }
  },
  set: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => {
    const set = await client.redis.zrange(id + '.' + field, 0, -1)
    if (set && set.length) {
      return set
    }
  },
  text: async (
    client: SelvaClient,
    id: string,
    field: string,
    lang?: string
  ) => {
    const all = await client.redis.selva_object_getall(id)
    const result: any = {}
    let hasFields = false
    Object.entries(all).forEach(([key, val]) => {
      if (key.startsWith(field + '.')) {
        hasFields = true
        setNestedResult(result, key.slice(field.length + 1), val)
      }
    })

    if (lang) {
      if (result[lang]) {
        return result[lang]
      }

      const allLangs = client.schemas.default.languages
      for (const l of allLangs) {
        if (result[l]) {
          return result[l]
        }
      }
    }

    if (hasFields) {
      return result
    }
  },
  object: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang?: string
  ) => {
    const all = await client.redis.selva_object_getall(id)
    const result: any = {}
    let hasKeys = false
    await Promise.all(
      Object.entries(all).map(async ([key, val]) => {
        if (key.startsWith(field + '.')) {
          hasKeys = true

          if (val === '___selva_$set') {
            const set = await client.redis.zrange(id + '.' + key, 0, -1)
            if (set) {
              setNestedResult(result, key.slice(field.length + 1), set)
            }
            return
          } else if (val === '___selva_$object') {
            return
          }

          const fieldSchema = getNestedSchema(client.schemas.default, id, key)
          const typeCast = TYPE_CASTS[fieldSchema.type]
          if (typeCast) {
            val = typeCast(val)
          }
          setNestedResult(result, key.slice(field.length + 1), val)
        }
      })
    )

    if (hasKeys) {
      return result
    }
  },
  record: async (
    client: SelvaClient,
    id: string,
    field: string,
    _lang: string
  ) => {
    return TYPE_TO_SPECIAL_OP.object(client, id, field, _lang)
  }
}

export const executeNestedGetOperations = async (
  client: SelvaClient,
  props: GetOptions,
  lang: string | undefined,
  ctx: ExecContext
): Promise<GetResult> => {
  const id = await resolveId(client, props)
  return await executeGetOperations(
    client,
    props.$language || lang,
    ctx,
    createGetOperations(client, props, id, '', ctx.db)
  )
}

// any is not so nice
export const executeGetOperation = async (
  client: SelvaClient,
  lang: string,
  ctx: ExecContext,
  op: GetOperation
): Promise<any> => {
  if (op.type === 'value') {
    return op.value
  } else if (op.type === 'nested_query') {
    return executeNestedGetOperations(client, op.props, lang, ctx)
  } else if (op.type === 'array_query') {
    return Promise.all(
      op.props.map(p => {
        if (p.$id) {
          return executeNestedGetOperations(client, p, lang, ctx)
        } else {
          return executeNestedGetOperations(
            client,
            Object.assign({}, p, { $id: op.id }),
            lang,
            ctx
          )
        }
      })
    )
  } else if (op.type === 'find') {
    return find(client, op, lang, ctx)
  } else if (op.type === 'inherit') {
    return inherit(client, op, lang, ctx)
  } else if (op.type === 'db') {
    const { db } = ctx
    console.log('GET', op)

    let r: any
    let fieldSchema
    if (Array.isArray(op.sourceField)) {
      fieldSchema = getNestedSchema(
        client.schemas.default,
        op.id,
        op.sourceField[0]
      )

      if (!fieldSchema) {
        return null
      }

      const specialOp = TYPE_TO_SPECIAL_OP[fieldSchema.type]

      const nested: GetOperation[] = await Promise.all(
        op.sourceField.map(async f => {
          bufferNodeMarker(ctx, op.id, f)
          if (specialOp) {
            return specialOp(client, op.id, f, lang)
          }

          return client.redis.selva_object_get({ name: db }, op.id, f)
        })
      )

      r = nested.find(x => !!x)
    } else {
      bufferNodeMarker(ctx, op.id, <string>op.sourceField)

      fieldSchema = getNestedSchema(
        client.schemas.default,
        op.id,
        op.sourceField
      )

      if (!fieldSchema) {
        return null
      }

      const specialOp = TYPE_TO_SPECIAL_OP[fieldSchema.type]
      if (specialOp) {
        r = await specialOp(client, op.id, op.sourceField, lang)
      } else {
        r = await client.redis.selva_object_get({ name: db }, op.id, op.sourceField)
      }
    }

    if (r !== null && r !== undefined) {
      const typeCast = TYPE_CASTS[fieldSchema.type]
      if (typeCast) {
        return typeCast(r)
      }

      return r
    } else if (op.default) {
      return op.default
    }
  } else {
    throw new Error(`Unsupported query type ${(<any>op).type}`)
  }
}

export default async function executeGetOperations(
  client: SelvaClient,
  lang: string | undefined,
  ctx: ExecContext,
  ops: GetOperation[]
): Promise<GetResult> {
  if (ctx.subId) {
    console.log('YEEEAH SUB ID', ctx)
  }
  const o: GetResult = {}
  const results = await Promise.all(
    ops.map(op => executeGetOperation(client, lang, ctx, op))
  )
  results.map((r, i) => {
    if (
      ops[i].field === '' &&
      // @ts-ignore
      ops[i].single === true
    ) {
      Object.assign(o, r)
    } else if (r !== null && r !== undefined) {
      setNestedResult(o, ops[i].field, r)
    }
  })

  // add buffered subscription markers
  await addNodeMarkers(client, ctx)

  return o
}
