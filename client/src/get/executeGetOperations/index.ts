import { SelvaClient } from '../../'
import { GetOperation, GetResult } from '../types'
import { setNestedResult, getNestedSchema } from '../utils'
import resolveId from '../resolveId'
import createGetOperations from '../createGetOperations'
import { GetOptions } from '../'
import find from './find'
import inherit from './inherit'
import { Rpn } from '@saulx/selva-query-ast-parser'
import { FieldSchemaArrayLike, Schema } from '~selva/schema'
import { ServerDescriptor } from '~selva/types'
import { makeLangArg } from './util'
import { deepCopy } from '@saulx/utils'

export type ExecContext = {
  db: string
  meta: any
  subId?: string
  originDescriptors?: Record<string, ServerDescriptor>
  nodeMarkers?: Record<string, Set<string>>
  hasFindMarkers?: boolean
}
export type SubscriptionMarker = {
  type: string // ancestors|descendants|<any other field traversed>
  id: string
  fields: string[]
  rpn?: Rpn
}

export function adler32(marker: SubscriptionMarker): number {
  const MOD_ADLER = 65521

  const str: string = JSON.stringify(marker)

  let a = 1
  let b = 0
  for (let i = 0; i < str.length; i++) {
    a = (a + str.charCodeAt(i)) % MOD_ADLER
    b = (b + a) % MOD_ADLER
  }

  const res = (b << 16) | a
  return res & 0x7fffffff
}

export async function addMarker(
  client: SelvaClient,
  ctx: ExecContext,
  marker: SubscriptionMarker
): Promise<boolean> {
  if (!ctx.subId) {
    return false
  }

  const markerId = adler32(marker)
  await client.redis.selva_subscriptions_add(
    ctx.originDescriptors[ctx.db] || { name: ctx.db },
    '___selva_hierarchy',
    ctx.subId,
    markerId,
    marker.type,
    marker.id,
    'fields',
    marker.fields.join('\n'),
    ...(marker.rpn ? marker.rpn : [])
  )

  return true
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

  fields.forEach((f) => current.add(f))

  ctx.nodeMarkers[id] = current
}

async function addNodeMarkers(
  client: SelvaClient,
  ctx: ExecContext
): Promise<number> {
  if (!ctx.nodeMarkers) {
    return
  }

  let count = 0
  try {
    await Promise.all(
      Object.entries(ctx.nodeMarkers).map(async ([id, fields]) => {
        count++
        return addMarker(client, ctx, {
          type: 'node',
          id,
          fields: [...fields.values()],
        })
      })
    )

    return count
  } catch (e) {
    console.error(e)
    return 0
  }
}

async function refreshMarkers(
  client: SelvaClient,
  ctx: ExecContext
): Promise<void> {
  if (!ctx.subId) {
    return
  }

  await client.redis.selva_subscriptions_refresh(
    ctx.originDescriptors[ctx.db] || { name: ctx.db },
    '___selva_hierarchy',
    ctx.subId
  )
}

export const TYPE_CASTS: Record<
  string,
  (x: any, id: string, field: string, schema: Schema, lang?: string) => any
> = {
  float: Number,
  number: Number,
  timestamp: Number,
  int: Number,
  boolean: (x: any) => !!Number(x),
  json: (x: any) => JSON.parse(x),
  // array: (x: any) => JSON.parse(x),
  array: (x: any, id: string, field: string, schema, lang) => {
    const fieldSchema = <FieldSchemaArrayLike>getNestedSchema(schema, id, field)
    if (!fieldSchema || !fieldSchema.items) {
      return x
    }

    if (['int', 'float', 'number'].includes(fieldSchema.items.type)) {
      const converted = x.map((num) => {
        if (typeof num === 'string') {
          return Number(num)
        } else {
          return num
        }
      })

      return converted
    } else if (
      ['object', 'record'].includes(fieldSchema.items.type) ||
      (!lang && fieldSchema.items.type === 'text')
    ) {
      const converted = x.map((el, i) => {
        if (el === null) {
          return null
        }

        return TYPE_CASTS.object(el, id, `${field}[${i}]`, schema, lang)
      })

      return converted
    } else if (fieldSchema.items.type === 'json') {
      const converted = x.map((el, i) => {
        return JSON.parse(el)
      })

      return converted
    }

    return x
  },
  set: (all: any, id: string, field: string, schema, lang) => {
    const fieldSchema = <FieldSchemaArrayLike>getNestedSchema(schema, id, field)
    if (!fieldSchema || !fieldSchema.items) {
      return all
    }

    if (
      ['number', 'int', 'float', 'timestamp'].includes(fieldSchema.items.type)
    ) {
      return all.map((x) => Number(x))
    }

    return all
  },
  object: (all: any, id: string, origField: string, schema, lang) => {
    const result = {}
    let fieldCount = 0
    const parse = (o, field: string, arr: string[]) =>
      arr.forEach((key, i, arr) => {
        const f = origField.includes('.*.')
          ? `${field.substr(0, field.indexOf('*') - 1)}.${key}`
          : `${field}.${key}`

        if ((i & 1) === 1) return
        let val = arr[i + 1]

        if (val === null) {
          return
        }

        let fieldSchema = getNestedSchema(schema, id, f)

        if (!fieldSchema) {
          throw new Error(
            'Cannot find field type ' + id + ` ${f} - getNestedSchema`
          )
        }

        // if (
        //   fieldSchema.type === 'array' &&
        //   (fieldSchema.items.type === 'object' ||
        //     fieldSchema.items.type === 'text' ||
        //     fieldSchema.items.type === 'record')
        // ) {
        //   fieldSchema = fieldSchema.items
        // }

        if (lang && 'text' === fieldSchema.type && Array.isArray(val)) {
          const txtObj = {}
          parse(txtObj, f, val)

          if (txtObj[lang]) {
            o[key] = txtObj[lang]
          } else {
            for (const l of schema.languages) {
              if (txtObj[l]) {
                o[key] = txtObj[l]
              }
            }
          }
        } else if (
          ['object', 'text'].includes(fieldSchema.type) &&
          Array.isArray(val)
        ) {
          o[key] = {}
          parse(o[key], f, val)
        } else {
          const typeCast = TYPE_CASTS[fieldSchema.type]
          if (typeCast) {
            val = typeCast(val, id, f, schema, lang)
          }

          setNestedResult(o, key, val)
        }

        fieldCount++
      })
    parse(result, origField, all)
    if (fieldCount) {
      return result
    }
  },
  record: (all: any, id: string, field: string, schema, lang?: string) => {
    // this is not a record... we are missing the field in between...
    return TYPE_CASTS.object(all, id, field, schema, lang)
  },
  text: (all: any, id: string, field: string, schema, lang) => {
    if (Array.isArray(all)) {
      const o = {}
      for (let i = 0; i < all.length; i += 2) {
        const key = all[i]
        const val = all[i + 1]

        o[key] = val
      }

      if (lang && o[lang]) {
        return o[lang]
      } else if (lang) {
        const allLangs = schema.languages
        for (const l of allLangs) {
          if (o[l]) {
            return o[l]
          }
        }

        return undefined
      }

      return o
    } else {
      return all
    }
  },
}

export function typeCast(
  x: any,
  id: string,
  field: string,
  schema: Schema,
  lang?: string
): any {
  if (field.includes('.*.')) {
    return TYPE_CASTS.record(x, id, field, schema, lang)
  }

  const fs = getNestedSchema(schema, id, field)
  if (!fs) {
    return x
  }

  const cast = TYPE_CASTS[fs.type]
  if (!cast) {
    return x
  }

  return cast(x, id, field, schema, lang)
}

const TYPE_TO_SPECIAL_OP: Record<
  string,
  (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    _lang?: string
  ) => Promise<any>
> = {
  id: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    _lang?: string
  ) => {
    return id
  },
  reference: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    lang?: string
  ) => {
    const r = await client.redis.selva_hierarchy_edgeget(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      '___selva_hierarchy',
      id,
      field
    )
    return r && r[1]
  },
  references: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    lang?: string
  ) => {
    const { db } = ctx
    const paddedId = id.padEnd(10, '\0')

    if (field === 'ancestors') {
      return client.redis.selva_hierarchy_find(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '',
        '___selva_hierarchy',
        'bfs',
        'ancestors',
        paddedId
      )
    } else if (field === 'descendants') {
      return client.redis.selva_hierarchy_find(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '',
        '___selva_hierarchy',
        'bfs',
        'descendants',
        paddedId
      )
    } else if (field === 'parents') {
      return client.redis.selva_hierarchy_parents(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '___selva_hierarchy',
        id
      )
    } else if (field === 'children') {
      return client.redis.selva_hierarchy_children(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '___selva_hierarchy',
        id
      )
    } else {
      const r = await client.redis.selva_hierarchy_edgeget(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        '___selva_hierarchy',
        id,
        field
      )
      if (!r || r.length == 1) {
        return null
      }
      r.shift()
      return r
    }
  },
  text: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    lang?: string
  ) => {
    const { db } = ctx

    let args = [makeLangArg(client.schemas[ctx.db].languages, lang), id]
    if (lang) {
      args.push(`${field}.${lang}`)
      if (client.schemas[db].languages) {
        args.push(...client.schemas[db].languages.map((l) => `${field}.${l}`))
      }
    } else {
      args.push(field)
    }
    const res = await client.redis.selva_object_get(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      ...args
    )
    if (res === null) {
      return null
    }
    if (lang) {
      return res
    } else {
      const o = {}
      for (let i = 0; i < res.length; i += 2) {
        o[res[i]] = res[i + 1]
      }
      return o
    }
  },
}

export const executeNestedGetOperations = async (
  client: SelvaClient,
  props: GetOptions,
  lang: string | undefined,
  ctx: ExecContext,
  runAsNested: boolean = true
): Promise<GetResult> => {
  const id = await resolveId(client, props)
  if (!id) return null
  return await executeGetOperations(
    client,
    props.$language || lang,
    ctx,
    createGetOperations(client, props, id, '', ctx.db),
    runAsNested
  )
}

// any is not so nice
export const executeGetOperation = async (
  client: SelvaClient,
  lang: string,
  ctx: ExecContext,
  op: GetOperation,
  nested?: boolean
): Promise<any> => {
  if (op.type === 'value') {
    return op.value
  } else if (op.type === 'nested_query') {
    if (op.id) {
      const field = op.sourceField
        ? Array.isArray(op.sourceField)
          ? op.sourceField
          : [op.sourceField]
        : [op.field]
      let id = op.fromReference
        ? await client.redis.selva_hierarchy_edgeget(
            ctx.originDescriptors[ctx.db] || { name: ctx.db },
            '___selva_hierarchy',
            op.id,
            ...field
          )
        : await client.redis.selva_object_get(
            ctx.originDescriptors[ctx.db] || { name: ctx.db },
            makeLangArg(client.schemas[ctx.db].languages, lang),
            op.id,
            ...field
          )
      if (op.fromReference && id) {
        id = id[1]
      }

      if (!id) {
        return null
      }

      const props = Object.assign({}, op.props, { $id: id })
      if (!op.props.$db && ctx.db && ctx.db !== 'default') {
        props.$db = ctx.db
      }

      return executeNestedGetOperations(
        client,
        props,
        lang,
        ctx,
        op.fromReference === true ? false : true
      )
    } else {
      const id = await resolveId(client, op.props)
      if (!id) return null
      return await executeGetOperations(
        client,
        op.props.$language || lang,
        ctx,
        createGetOperations(client, op.props, id, '', ctx.db)
      )
    }
  } else if (op.type === 'array_query') {
    return Promise.all(
      op.props.map((p) => {
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

    let r: any
    let fieldSchema

    if (Array.isArray(op.sourceField)) {
      fieldSchema = getNestedSchema(
        client.schemas[db],
        op.id,
        op.sourceField[0]
      )

      if (!fieldSchema) {
        return null
      }

      const specialOp = TYPE_TO_SPECIAL_OP[fieldSchema.type]

      const all: GetOperation[] = await Promise.all(
        op.sourceField.map(async (f) => {
          if (!nested) {
            bufferNodeMarker(ctx, op.id, f)
          }
          if (specialOp) {
            return specialOp(client, ctx, op.id, f, lang)
          }

          return client.redis.selva_object_get(
            ctx.originDescriptors[ctx.db] || { name: ctx.db },
            makeLangArg(client.schemas[ctx.db].languages, lang),
            op.id,
            f
          )
        })
      )

      r = all.find((x) => !!x)
    } else {
      if (!nested) {
        bufferNodeMarker(ctx, op.id, <string>op.sourceField)
      }

      fieldSchema = getNestedSchema(client.schemas[db], op.id, op.sourceField)

      if (!fieldSchema) {
        return null
      }

      const specialOp = TYPE_TO_SPECIAL_OP[fieldSchema.type]
      if (specialOp) {
        r = await specialOp(client, ctx, op.id, op.sourceField, lang)
      } else {
        r = await client.redis.selva_object_get(
          ctx.originDescriptors[ctx.db] || { name: ctx.db },
          makeLangArg(client.schemas[ctx.db].languages, lang),
          op.id,
          op.sourceField
        )
      }
    }

    if (r !== null && r !== undefined) {
      return typeCast(
        r,
        op.id,
        Array.isArray(op.sourceField) ? op.sourceField[0] : op.sourceField,
        client.schemas[ctx.db],
        lang
      )
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
  ops: GetOperation[],
  nested?: boolean
): Promise<GetResult> {
  const o: GetResult = {}

  const results = await Promise.all(
    ops.map((op) => executeGetOperation(client, lang, ctx, op, nested))
  )
  results.map((r, i) => {
    if (
      ops[i].field === '' &&
      // @ts-ignore
      ops[i].single === true
    ) {
      Object.assign(o, r)
    } else if (r !== null && r !== undefined) {
      if (ops[i].field.includes('.*.')) {
        setNestedResult(
          o,
          ops[i].field.substr(0, ops[i].field.indexOf('*') - 1),
          r
        )
      } else {
        setNestedResult(o, ops[i].field, r)
      }
    }
  })

  // add buffered subscription markers
  if (!nested) {
    const addedMarkers = await addNodeMarkers(client, ctx)
    if (addedMarkers || ctx.hasFindMarkers) {
      await refreshMarkers(client, ctx)
    }
  }

  return o
}
