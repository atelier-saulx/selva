import { readValue } from 'data-record'
import { SelvaClient } from '../../'
import { GetOperation, GetResult, TraverseByType } from '../types'
import { FieldSchema } from '../../schema'
import { setNestedResult, getNestedSchema, getNestedField } from '../utils'
import resolveId from '../resolveId'
import createGetOperations from '../createGetOperations'
import { doubleDef, longLongDef } from '../../set/modifyDataRecords'
import { GetOptions } from '../'
import find from './find'
import aggregate from './aggregate'
import inherit from './inherit'
import timeseries from './timeseries'
import { Rpn, bfsExpr2rpn } from '@saulx/selva-query-ast-parser'
import { FieldSchemaArrayLike, Schema } from '~selva/schema'
import { ServerDescriptor } from '~selva/types'
import { makeLangArg } from './util'
import { deepMerge } from '@saulx/utils'
import { padId } from '../../util'

export type ExecContext = {
  db: string
  meta: any
  subId?: string
  originDescriptors?: Record<string, ServerDescriptor>
  firstEval?: boolean
  nodeMarkers?: Record<string, Set<string>>
  hasFindMarkers?: boolean
}

export type TraversalType =
  | 'none'
  | 'node'
  | 'array'
  | 'children'
  | 'parents'
  | 'ancestors'
  | 'descendants'
  | 'ref'
  | 'edge_field'
  | 'bfs_edge_field'
  | 'bfs_expression'
  | 'expression'

export type SubscriptionMarker = {
  type: TraversalType
  refField?: string
  traversal?: string // an RPN for bfs_expression
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

export function sourceFieldToDir(
  schema: Schema,
  fieldSchema: FieldSchema,
  field: string | string[],
  recursive: boolean,
  byType?: TraverseByType
): { type: TraversalType; refField?: string } {
  if (byType) {
    return {
      type: recursive ? 'bfs_expression' : 'expression',
      refField: bfsExpr2rpn(schema.types, byType),
    }
  }

  if (Array.isArray(field)) {
    return {
      type: recursive ? 'bfs_expression' : 'expression',
      refField: bfsExpr2rpn(schema.types, {
        $any: { $first: field },
      }),
    }
  }

  const defaultFields: Array<TraversalType> = [
    'children',
    'parents',
    'ancestors',
    'descendants',
  ]
  if (defaultFields.includes(field as TraversalType)) {
    return {
      type: field as TraversalType,
    }
  } else if (fieldSchema && fieldSchema.type === 'array') {
    return {
      type: 'array',
      refField: field,
    }
  } else {
    return {
      type: fieldSchema && fieldSchema.type === 'string' ? 'ref' : 'edge_field',
      refField: field,
    }
  }
}

export function sourceFieldToFindArgs(
  schema: Schema,
  fieldSchema: FieldSchema | null,
  sourceField: string | string[],
  recursive: boolean,
  byType?: TraverseByType
): [SubscriptionMarker['type'], string?] {
  if (byType) {
    return [
      recursive ? 'bfs_expression' : 'expression',
      bfsExpr2rpn(schema.types, byType),
    ]
  }

  if (Array.isArray(sourceField)) {
    return [
      recursive ? 'bfs_expression' : 'expression',
      bfsExpr2rpn(schema.types, {
        $any: { $first: sourceField },
      }),
    ]
  }

  if (
    ['ancestors', 'descendants', 'children', 'parents'].includes(sourceField)
  ) {
    return [<SubscriptionMarker['type']>sourceField]
  }

  // if fieldSchema is null it usually means that the caller needs to do an op
  // over multiple nodes and thus it's not possible to determine an optimal
  // hierarchy traversal method. We'll fallback to bfs_expression.
  if (!fieldSchema) {
    return ['bfs_expression', `{"${sourceField}"}`]
  }

  const t = sourceFieldToDir(
    schema,
    fieldSchema,
    sourceField,
    recursive,
    byType
  )
  return recursive && t.refField
    ? ['bfs_expression', `{"${sourceField}"}`]
    : t.refField
    ? [t.type, t.refField]
    : [t.type]
}

export async function addMarker(
  client: SelvaClient,
  ctx: ExecContext,
  marker: SubscriptionMarker,
  passedOnSchema?: Schema
): Promise<boolean> {
  if (!ctx.subId) {
    return false
  }

  // const shouldHaveRef = ['ref', 'bfs_edge_field'].includes(marker.type)
  // if (shouldHaveRef && (!marker.refField || marker.traversal) ||
  //    !shouldHaveRef && marker.refField) {
  //    throw new Error(`Invalid params for a "${marker.type}" marker`)
  // }

  // const shouldHaveTraversal = marker.type == 'bfs_expression'
  // if (shouldHaveTraversal && (!marker.traversal || marker.refField) ||
  //    !shouldHaveTraversal && marker.traversal) {
  //    throw new Error(`Invalid params for a "${marker.type}" marker`)
  // }

  const schema = passedOnSchema || client.schemas[ctx.db]
  const fieldSchema = getNestedSchema(schema, marker.id, marker.refField)
  if (fieldSchema && fieldSchema.type === 'array') {
    const nestedFields = marker.fields.map((f) => {
      return `${marker.refField}[n].${f}`
    })
    return addMarker(
      client,
      ctx,
      {
        type: 'node',
        id: marker.id,
        fields: nestedFields,
      },
      passedOnSchema
    )
  }

  const markerId = adler32(marker)
  const markerType = [marker.type, marker.refField, marker.traversal].filter(
    (v) => v
  )
  await client.redis.selva_subscriptions_add(
    ctx.originDescriptors[ctx.db] || { name: ctx.db },
    '___selva_hierarchy',
    ctx.subId,
    markerId,
    ...markerType,
    marker.id,
    'fields',
    marker.fields
      .map((f) => {
        const idx = f.indexOf('.*.')
        if (idx > 0) {
          return f.slice(0, idx)
        }

        return f
      })
      .join('\n'),
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
  ctx: ExecContext,
  passedOnSchema?: Schema
): Promise<number> {
  if (!ctx.nodeMarkers) {
    return
  }

  let count = 0
  try {
    await Promise.all(
      Object.entries(ctx.nodeMarkers).map(async ([id, fields]) => {
        count++
        return addMarker(
          client,
          ctx,
          {
            type: 'node',
            id,
            fields: [...fields.values()],
          },
          passedOnSchema
        )
      })
    )

    return count
  } catch (e) {
    console.error('Adding a marker failed:', e)
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

export function readDouble(x: string) {
  return readValue(doubleDef, Buffer.from(x), '.d')
}

export function readLongLong(x: string) {
  return readValue(longLongDef, Buffer.from(x), '.d')
}

export const TYPE_CASTS: Record<
  string,
  (x: any, id: string, field: string, schema: Schema, lang?: string) => any
> = {
  float: (x: any) => readDouble(x),
  number: (x: any) => readDouble(x),
  timestamp: (x: any) => Number(readLongLong(x)),
  int: (x: any) => Number(readLongLong(x)),
  boolean: (x: any) => !!Number(readLongLong(x)),
  json: (x: any) => JSON.parse(x),
  reference: (r: any, _id: string, _field: string, schema, lang) => {
    if (Array.isArray(r) && r.length > 1) {
      const idIdx = r.findIndex((x) => x === 'id')
      const refId = r[idIdx + 1]

      const totalResult = {}
      for (let i = 0; i < r.length; i += 2) {
        const field = r[i]
        const value = r[i + 1]

        const fieldResult = typeCast(value, refId, field, schema, lang)
        setNestedResult(totalResult, field, fieldResult)
      }

      return totalResult
    }

    return Array.isArray(r) ? r[0] : r
  },
  array: (x: any, id: string, field: string, schema, lang) => {
    const fieldSchema = <FieldSchemaArrayLike>getNestedSchema(schema, id, field)
    if (!fieldSchema || !fieldSchema.items) {
      return x
    }

    if (
      ['boolean', 'float', 'number', 'timestamp', 'int'].includes(
        fieldSchema.items.type
      )
    ) {
      return x.map((num) =>
        TYPE_CASTS[fieldSchema.items.type](num, id, '', schema, lang)
      )
    } else if (
      ['object', 'record'].includes(fieldSchema.items.type) ||
      (!lang && fieldSchema.items.type === 'text')
    ) {
      const converted = x.map((el, i) => {
        if (el === null || el === undefined || !el.length) {
          return {}
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
      ['float', 'number', 'timestamp', 'int'].includes(fieldSchema.items.type)
    ) {
      return all.map((num) =>
        TYPE_CASTS[fieldSchema.items.type](num, id, '', schema, lang)
      )
    }

    return all
  },
  object: (all: any, id: string, origField: string, schema, lang) => {
    const result: Record<string, any> = {}
    let fieldCount = 0
    const parse = (o, field: string, arr: string[]) =>
      arr.forEach((key, i, arr) => {
        const f = field.includes('.*.')
          ? `${field.substring(0, field.indexOf('*') - 1)}.${key}`
          : `${field}.${key}`

        if ((i & 1) === 1) return
        let val = arr[i + 1]

        if (val === null) {
          return
        }

        const fieldSchema = getNestedSchema(schema, id, f)
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

        if (lang && fieldSchema.type === 'text' && Array.isArray(val)) {
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
          if (!o[key]) {
            o[key] = {}
          }
          parse(o[key], f, val)
        } else {
          val = typeCast(val, id, f, schema, lang)
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

  if (fs.timeseries) {
    const vIdx = x.findIndex((el) => {
      return el === '_value'
    })

    x = x[vIdx + 1]
  }

  const cast = TYPE_CASTS[fs.type]
  if (!cast) {
    return x
  }

  return cast(x, id, field, schema, lang)
}

function findNodeRes2field(field: string, findRes: any) {
  const fields = findRes[0] ? findRes[0][1] : []
  let out = []

  for (let i = 0; i < fields.length; i += 2) {
    if (fields[i] === field) {
      out.push(...fields[i + 1])
    } else /* if (fields[i].substring(0, field.length) === field) */ {
      out.push(
        fields[i].substring(field.length + 1),
        fields[i + 1]
      )
    }
  }

  return out
}

function findNodeRes2array(field: string, findRes: any) {
  const fields = findRes[0] ? findRes[0][1] : []
  let out = []

  if (field.includes('.*.')) {
    for (let i = 0; i < fields.length; i += 2) {
      out.push(
        fields[i].substring(field.indexOf('*')),
        fields[i + 1]
      )
    }
  } else {
    for (let i = 0; i < fields.length; i += 2) {
      out.push(...fields[i + 1])
    }
  }

  return out
}

const TYPE_TO_SPECIAL_OP: Record<
  string,
  (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    _lang?: string,
    schema?: Schema
  ) => Promise<any>
> = {
  id: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    lang?: string,
    schema?: Schema
  ) => {
    return id
  },
  reference: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    lang?: string,
    schema?: Schema
  ) => {
    const paddedId = padId(id)
    const { db } = ctx

    const r = await client.redis.selva_hierarchy_find(
      ctx.originDescriptors[db] || { name: db },
      '',
      '___selva_hierarchy',
      'node',
      'fields', field,
      padId(id)
    )

    return findNodeRes2array(field, r)
  },
  references: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    lang?: string,
    schema?: Schema
  ) => {
    const paddedId = padId(id)
    const { db } = ctx

    const r = await client.redis.selva_hierarchy_find(
      ctx.originDescriptors[db] || { name: db },
      '',
      '___selva_hierarchy',
      'node',
      'fields', field,
      padId(id)
    )

    return findNodeRes2array(field, r)
  },
  text: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    lang?: string,
    schema?: Schema
  ) => {
    const { db } = ctx

    if (!schema) {
      schema = client.schemas[db]
    }

    const args = [makeLangArg(schema.languages, lang), id]
    if (lang) {
      args.push(`${field}.${lang}`)
      if (schema.languages) {
        args.push(...schema.languages.map((l) => `${field}.${l}`))
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
  record: async (
    client: SelvaClient,
    ctx: ExecContext,
    id: string,
    field: string,
    lang?: string,
    schema?: Schema
  ) => {
    const { db } = ctx

    const r = await client.redis.selva_hierarchy_find(
      ctx.originDescriptors[db] || { name: db },
      '',
      '___selva_hierarchy',
      'node',
      'fields', field,
      padId(id)
    )

    return findNodeRes2field(field, r)
  }
}

export const executeNestedGetOperations = async (
  client: SelvaClient,
  props: GetOptions,
  lang: string | undefined,
  ctx: ExecContext,
  runAsNested: boolean = true,
  schema?: Schema
): Promise<GetResult> => {
  const id = await resolveId(client, props)
  if (!id) return null
  return await executeGetOperations(
    client,
    props.$language || lang,
    ctx,
    createGetOperations(client, props, id, '', ctx.db, undefined, schema),
    runAsNested,
    schema
  )
}

// any is not so nice
export const executeGetOperation = async (
  client: SelvaClient,
  lang: string,
  ctx: ExecContext,
  op: GetOperation,
  nested?: boolean,
  schema?: Schema
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
        ? await TYPE_TO_SPECIAL_OP.reference(client, ctx, op.id, field.join('\n'), lang, schema)
        : await client.redis.selva_object_get(
            ctx.originDescriptors[ctx.db] || { name: ctx.db },
            makeLangArg((schema || client.schemas[ctx.db]).languages, lang),
            op.id,
            ...field
          )

      if (op.fromReference) {
        bufferNodeMarker(ctx, op.id, ...field)
        await Promise.all(
          field.map((f) => {
            return addMarker(
              client,
              ctx,
              {
                id: op.id,
                type: 'edge_field',
                refField: f, // TODO: use expression?
                fields: Object.keys(op.props).filter((f) => !f.startsWith('$')),
              },
              schema
            )
          })
        )

        if (id) {
          if (typeof id[1] === 'string') {
            id = id[1]
          } else {
            // references from a wildcard record
            let tmp = {}
            for (let i = 0; i < id.length; i += 2) {
              tmp[id[i]] = id[i + 1]
            }
            id = tmp
          }
        }
      }

      const execGetOp = (props) => executeNestedGetOperations(
        client,
        props,
        lang,
        ctx,
        // eslint-disable-next-line
        op.fromReference === true ? false : true,
        schema
      )

      if (typeof id === 'string') {
        const props = Object.assign({}, op.props, { $id: id })

        if (!op.props.$db && ctx.db && ctx.db !== 'default') {
          props.$db = ctx.db
        }

        return execGetOp(props)
      } else if (typeof id === 'object') {
        const res = {}

        await Promise.all(Object.keys(id).map(async (key) => {
          const props = Object.assign({}, op.props, { $id: id[key] })

          if (!op.props.$db && ctx.db && ctx.db !== 'default') {
            props.$db = ctx.db
          }

          setNestedResult(res, key, await execGetOp(props))
        }))

        return res;
      } else {
          return null;
      }

    } else {
      const id = await resolveId(client, op.props)
      if (!id) return null
      return await executeGetOperations(
        client,
        op.props.$language || lang,
        ctx,
        createGetOperations(
          client,
          op.props,
          id,
          '',
          ctx.db,
          undefined,
          schema
        ),
        false,
        schema
      )
    }
  } else if (op.type === 'array_query') {
    return Promise.all(
      op.props.map((p) => {
        if (p.$id) {
          return executeNestedGetOperations(client, p, lang, ctx, false, schema)
        } else {
          return executeNestedGetOperations(
            client,
            Object.assign({}, p, { $id: op.id }),
            lang,
            ctx,
            false,
            schema
          )
        }
      })
    )
  } else if (op.type === 'find') {
    if (op.isTimeseries) {
      return timeseries(client, op, lang, ctx)
    } else {
      return find(client, op, lang, ctx, schema)
    }
  } else if (op.type === 'aggregate') {
    if (op.isTimeseries) {
      // add this later...
      return timeseries(client, op, lang, ctx)
    } else {
      return aggregate(client, op, lang, ctx, schema)
    }
  } else if (op.type === 'inherit') {
    return inherit(client, op, lang, ctx, schema)
  } else if (op.type === 'raw') {
    return await client.redis.selva_object_get(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      '',
      op.id,
      <string>op.sourceField
    )
  } else if (op.type === 'db') {
    const { db } = ctx

    let r: any
    let fieldSchema

    if (Array.isArray(op.sourceField)) {
      fieldSchema = getNestedSchema(
        schema || client.schemas[db],
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
            return specialOp(client, ctx, op.id, f, lang, schema)
          }

          return client.redis.selva_object_get(
            ctx.originDescriptors[ctx.db] || { name: ctx.db },
            makeLangArg((schema || client.schemas[ctx.db]).languages, lang),
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

      fieldSchema = getNestedSchema(
        schema || client.schemas[db],
        op.id,
        op.sourceField
      )

      if (!fieldSchema) {
        return null
      }

      const specialOp = TYPE_TO_SPECIAL_OP[fieldSchema.type]
      if (specialOp) {
        r = await specialOp(client, ctx, op.id, op.sourceField, lang)
      } else {
        r = await client.redis.selva_object_get(
          ctx.originDescriptors[ctx.db] || { name: ctx.db },
          makeLangArg((schema || client.schemas[ctx.db]).languages, lang),
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
        schema || client.schemas[ctx.db],
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
  nested?: boolean,
  schema?: Schema
): Promise<GetResult> {
  const o: GetResult = {}

  const results = await Promise.all(
    ops.map((op) => executeGetOperation(client, lang, ctx, op, nested, schema))
  )
  results.forEach((r, i) => {
    if (
      ops[i].field === '' &&
      // @ts-ignore
      ops[i].single === true
    ) {
      Object.assign(o, r)
    } else if (r !== null && r !== undefined) {
      if (ops[i].field.includes('.*.')) {
        const f = ops[i].field.substring(0, ops[i].field.indexOf('*') - 1)
        const current = getNestedField(o, f)
        setNestedResult(o, f, deepMerge({}, current, r))
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
