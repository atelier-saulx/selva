import { Schema, SelvaClient } from '../../'
import {
  GetOperationFind,
  GetResult,
  GetOptions,
  TraverseByType,
} from '../types'
import {
  readLongLong,
  sourceFieldToFindArgs,
  typeCast,
  executeNestedGetOperations,
  ExecContext,
  sourceFieldToDir,
  bufferFindMarker,
} from './'
import {
  ast2rpn,
  Fork,
  FilterAST,
  isFork,
  convertNow,
  bfsExpr2rpn,
} from '@saulx/selva-query-ast-parser'

import { padId, joinIds, EMPTY_ID, NODE_ID_SIZE } from '../../util'
import { setNestedResult, getNestedSchema } from '../utils'
import { makeLangArg } from './util'
import { mkIndex } from './indexing'

function makeFieldsString(
  schema: Schema,
  fieldsByType: Map<string, Set<string>>,
  isInherit: boolean = false
): [string, string] {
  const fields = fieldsByType.get('$any')

  if (
    isInherit ||
    fieldsByType.size > 1 ||
    (fieldsByType.size === 1 && !fieldsByType.has('$any'))
  ) {
    const any = fieldsByType.get('$any') || new Set()

    const byType: TraverseByType = {
      $any: any.size ? { $all: [...any] } : false,
    }

    for (const [type, tFields] of fieldsByType.entries()) {
      if (type === '$any') {
        continue
      }

      const allFields = new Set([...tFields])
      for (const aField of any) {
        allFields.add(aField)
      }

      if (!allFields.size) {
        continue
      }

      byType[type] = { $all: [...allFields] }
    }

    return [
      isInherit ? 'inherit_rpn' : 'fields_rpn',
      bfsExpr2rpn(schema.types, byType),
    ]
  }

  let str = ''
  let hasWildcard = false
  for (const f of fields) {
    if (f === '*') {
      hasWildcard = true
      continue
    }

    str += f + '\n'
  }

  if (hasWildcard) {
    str += '*'
  } else {
    str = str.slice(0, -1)
  }

  return ['fields', str]
}

function parseGetOpts(
  schema: Schema,
  props: GetOptions,
  path: string,
  type: string = '$any',
  nestedMapping?: Record<string, { targetField?: string[]; default?: any }>
): [
  Map<string, Set<string>>,
  Record<
    string,
    { targetField?: string[]; default?: any; isInherit?: boolean }
  >,
  boolean,
  boolean
] {
  let isInherit = false
  const pathPrefix = path === '' ? '' : path + '.'
  const fields: Map<string, Set<string>> = new Map()
  if (!fields.has(type)) {
    fields.set(type, new Set())
  }
  const mapping: Record<
    string,
    {
      targetField?: string[]
      default?: any
      isInherit?: boolean
    }
  > = nestedMapping || {}

  const $fieldOpt =
    props.$list && props.$field && pathPrefix === '' ? undefined : props.$field

  const addMapping = ($field: string | string[], path: string) => {
    if (!$field) {
      return
    }

    if (Array.isArray($field)) {
      fields.get(type).add($field.join('|'))
      $field.forEach((f) => {
        if (!mapping[f]) {
          mapping[f] = { targetField: [path] }
          return
        }
        if (!mapping[f].targetField) {
          mapping[f].targetField = [path]
          return
        }
        mapping[f].targetField.push(path)
      })
    } else {
      fields.get(type).add($field)
      if (!mapping[$field]) {
        mapping[$field] = { targetField: [path] }
      } else if (!mapping[$field].targetField) {
        mapping[$field].targetField = [path]
      } else {
        mapping[$field].targetField.push(path)
      }
    }
  }

  let hasKeys = false
  for (const k in props) {
    if ((k === '$list' || k === '$find') && pathPrefix === '') {
      // ignore
    } else if (props.$list && k === '$field' && pathPrefix === '') {
      // ignore
    } else if (!k.startsWith('$') && props[k] === true) {
      if ($fieldOpt) {
        addMapping($fieldOpt + '.' + k, pathPrefix + k)
      } else {
        fields.get(type).add(pathPrefix + k)
      }
    } else if (props[k] === false) {
      fields.get(type).add(`!${pathPrefix + k}`)
    } else if (k === '$inherit') {
      const asAny = props[k] as any

      if (asAny.$item) {
        return [fields, mapping, true, false]
      } else if (asAny.$merge === true) {
        return [fields, mapping, true, false]
      }

      let $field = props.$field as string
      addMapping($field, path)
      if (!$field) {
        $field = path
      }

      const objKeys = Object.keys(props)
      if (objKeys.some((key) => !key.startsWith('$'))) {
        return [fields, mapping, true, false]
      }

      let types = asAny?.$type
      if (!types) {
        types = []
      } else if (typeof types === 'string') {
        types = [types]
      }

      const prefixes = types.map((type: string) => {
        if (type === 'root') {
          return 'ro'
        }

        return schema.types[type].prefix
      })

      fields.get(type).add(`^${prefixes.join('')}:${$field}`)
      if (mapping[$field]) {
        mapping[$field].isInherit = true
      } else {
        mapping[$field] = { isInherit: true }
      }

      isInherit = true
    } else if (k === '$field') {
      // no-op
    } else if (k === '$default') {
      fields.get(type).add(path)
      const $default = props[k]
      if (!mapping[path]) {
        mapping[path] = { default: $default }
      } else {
        mapping[path].default = $default
      }
    } else if (path === '' && k === '$all') {
      fields.get(type).add('*')
    } else if (k === '$all') {
      fields.get(type).add(path + '.*')
    } else if (k === '$fieldsByType') {
      for (const t in props.$fieldsByType) {
        const [nestedFieldsMap, , hasSpecial, nestedInherit] = parseGetOpts(
          schema,
          props.$fieldsByType[t],
          path,
          t,
          nestedMapping
        )
        if (hasSpecial) {
          return [fields, mapping, true, false]
        }
        if (nestedInherit) {
          isInherit = true
        }
        for (const [type, nestedFields] of nestedFieldsMap.entries()) {
          const set = fields.get(type) || new Set()
          for (const f of nestedFields.values()) {
            set.add(f)
          }
          fields.set(type, set)
        }
      }
    } else if (k.startsWith('$')) {
      return [fields, mapping, true, false]
    } else if (typeof props[k] === 'object') {
      const nestedProps = Object.assign({}, props[k])
      if ($fieldOpt) {
        nestedProps.$field = $fieldOpt + '.' + k
      }

      const [nestedFieldsMap, , hasSpecial, nestedInherit] = parseGetOpts(
        schema,
        props[k],
        pathPrefix + k,
        type,
        mapping
      )
      if (hasSpecial) {
        return [fields, mapping, true, false]
      }
      if (nestedInherit) {
        isInherit = true
      }

      for (const [type, nestedFields] of nestedFieldsMap.entries()) {
        const set = fields.get(type) || new Set()
        for (const f of nestedFields.values()) {
          set.add(f)
        }
        fields.set(type, set)
      }
    }

    if (k !== '$field') {
      hasKeys = true
    }
  }

  if (!hasKeys) {
    addMapping($fieldOpt, path)
  }

  return [fields, mapping, false, isInherit]
}

function findTimebased(ast: Fork): FilterAST[] {
  if (!ast) {
    return []
  }

  const parse = (fork: Fork, filters: FilterAST[]) => {
    if (fork.$and) {
      for (const f of fork.$and) {
        if (isFork(f)) {
          parse(f, filters)
        } else if (f.hasNow) {
          filters.push(f)
        }
      }
    } else if (fork.$or) {
      for (const f of fork.$or) {
        if (isFork(f)) {
          parse(f, filters)
        } else if (f.hasNow) {
          filters.push(f)
        }
      }
    }
  }

  const res = []
  parse(ast, res)
  return res
}

function excludeTimebased(ast: Fork | FilterAST): Fork | FilterAST {
  if (!isFork(ast)) {
    return ast
  }

  const newFork = Object.assign({}, ast)
  const filters = []
  if (ast.$or) {
    for (const f of ast.$or) {
      if (isFork(f)) {
        const n = excludeTimebased(f)
        if (n) {
          filters.push(n)
        }
      } else if (!f.hasNow) {
        filters.push(f)
      }
    }

    newFork.$or = filters
  } else if (ast.$and) {
    for (const f of ast.$and) {
      if (isFork(f)) {
        const n = excludeTimebased(f)
        if (n) {
          filters.push(n)
        }
      } else if (!f.hasNow) {
        filters.push(f)
      }
    }

    newFork.$and = filters
  }

  if (!filters.length) {
    return null
  }

  return newFork
}

async function checkForNextRefresh(
  ctx: ExecContext,
  client: SelvaClient,
  sourceField: string,
  paddedIds: string,
  ast: Fork,
  lang?: string,
  passedSchema?: Schema
): Promise<void> {
  if (!ctx.subId) {
    return
  }

  const uniq = new Set()
  const timebased = findTimebased(ast).filter((f) => {
    if (uniq.has(f.$field)) {
      return false
    }

    uniq.add(f.$field)
    return true
  })

  if (!timebased.length) {
    return
  }

  const withoutTimebased = excludeTimebased(ast)
  await Promise.all(
    timebased.map(async (f) => {
      // console.log('TRYING TIMEBASED')
      const newFilter: FilterAST = {
        $operator: '>',
        $value: f.$value,
        $field: f.$field,
      }

      const newFork: Fork = {
        isFork: true,
        $and: [withoutTimebased, newFilter],
      }

      if (!withoutTimebased) {
        newFork.$and = [newFilter]
      }

      const args = ast2rpn(
        (passedSchema || client.schemas[ctx.db]).types,
        newFork,
        lang
      )
      const ids = await client.redis.selva_hierarchy_find(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        makeLangArg((passedSchema || client.schemas[ctx.db]).languages, lang),
        '___selva_hierarchy',
        // TODO: needs byType expression
        ...sourceFieldToFindArgs(
          passedSchema || client.schemas[ctx.db],
          null,
          sourceField,
          false
        ),
        // TODO: needs indexing
        'order',
        f.$field,
        'asc',
        'offset',
        0,
        'limit',
        1,
        paddedIds,
        ...args
      )

      if (!ids || !ids.length) {
        return
      }

      const [id] = ids

      const time = Number(
        readLongLong(
          await client.redis.selva_object_get(
            ctx.originDescriptors[ctx.db] || { name: ctx.db },
            makeLangArg(
              (passedSchema || client.schemas[ctx.db]).languages,
              lang
            ),
            id,
            f.$field
          )
        )
      )

      let v = <string>f.$value
      if (v.startsWith('now-')) {
        v = v.replace('now-', 'now+')
      } else if (v.startsWith('now+')) {
        v = v.replace('now+', 'now-')
      }

      const converted = convertNow(v, time)
      // console.log('TIME NOW', Date.now())
      // console.log('NEXT TIME', time)
      // console.log('ADJUSTED', converted)

      if (!ctx.meta.___refreshAt || ctx.meta.___refreshAt > converted) {
        ctx.meta.___refreshAt = converted
      }
    })
  )
}

export const findIds = async (
  client: SelvaClient,
  op: GetOperationFind,
  lang: string,
  ctx: ExecContext,
  passedSchema?: Schema
): Promise<string[]> => {
  const sourceField: string = <string>op.sourceField
  if (typeof op.props.$list === 'object' && op.props.$list.$inherit) {
    const res = await executeNestedGetOperations(
      client,
      {
        $db: ctx.db,
        $id: op.id,
        result: {
          $field: op.sourceField,
          $inherit: op.props.$list.$inherit,
        },
      },
      lang,
      ctx,
      false,
      passedSchema
    )

    op.inKeys = res.result
  }

  const args = op.filter
    ? ast2rpn(client.schemas[ctx.db].types, op.filter, lang)
    : ['#1']
  // TODO: change this if ctx.subId (for markers)
  if (op.inKeys) {
    // can make this a bit better....
    const ids = await client.redis.selva_hierarchy_find(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      makeLangArg(client.schemas[ctx.db].languages, lang),
      '___selva_hierarchy',
      'node',
      'order',
      op.options.sort?.$field || '',
      op.options.sort?.$order || 'none',
      'offset',
      op.options.offset,
      'limit',
      op.options.limit,
      joinIds(op.inKeys),
      ...args
    )

    await checkForNextRefresh(
      ctx,
      client,
      sourceField,
      joinIds(op.inKeys),
      op.filter,
      lang
    )

    return ids
  } else {
    const realOpts: any = {}
    for (const key in op.props) {
      if (!key.startsWith('$')) {
        realOpts[key] = true
      }
    }

    if (op.nested) {
      let added = false
      for (let i = 0; i < op.id.length; i += NODE_ID_SIZE) {
        let endLen = NODE_ID_SIZE
        while (op.id[i + endLen - 1] === '\0') {
          endLen--
        }
        const id = op.id.substr(i, endLen)
        const schema = client.schemas[ctx.db]
        const sourceFieldSchema = getNestedSchema(schema, id, sourceField)
        bufferFindMarker(ctx, {
          ...sourceFieldToDir(
            schema,
            sourceFieldSchema,
            sourceField,
            op.recursive,
            op.byType
          ),
          id: id,
          fields: op.props.$all === true ? [] : Object.keys(realOpts),
          rpn: args,
        })

        await checkForNextRefresh(ctx, client, sourceField, id, op.filter, lang)
      }
    } else {
      const schema = client.schemas[ctx.db]
      for (let i = 0; i < op.id.length; i += NODE_ID_SIZE) {
        let endLen = NODE_ID_SIZE
        while (op.id[i + endLen - 1] === '\0') {
          endLen--
        }
        const id = op.id.substr(i, endLen)
        const sourceFieldSchema = getNestedSchema(schema, id, sourceField)
        bufferFindMarker(ctx, {
          ...sourceFieldToDir(
            schema,
            sourceFieldSchema,
            sourceField,
            op.recursive,
            op.byType
          ),
          id,
          fields: op.props.$all === true ? [] : Object.keys(realOpts),
          rpn: args,
        })
      }
    }

    const schema = client.schemas[ctx.db]
    const sourceFieldSchema = op.nested
      ? null
      : getNestedSchema(schema, op.id, sourceField)
    const ids = await client.redis.selva_hierarchy_find(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      makeLangArg(schema.languages, lang),
      '___selva_hierarchy',
      ...sourceFieldToFindArgs(
        schema,
        sourceFieldSchema,
        sourceField,
        op.recursive,
        op.byType
      ),
      ...mkIndex(schema, op),
      'order',
      op.options.sort?.$field || '',
      op.options.sort?.$order || 'none',
      'offset',
      op.options.offset,
      'limit',
      op.options.limit,
      padId(op.id),
      ...args
    )

    await checkForNextRefresh(
      ctx,
      client,
      sourceField,
      padId(op.id),
      op.filter,
      lang
    )

    return ids
  }
}

const findFields = async (
  client: SelvaClient,
  op: GetOperationFind,
  lang: string,
  ctx: ExecContext,
  fieldsOpt: Map<string, Set<string>>,
  isInherit: boolean = false,
  passedSchema?: Schema
): Promise<string[]> => {
  const sourceField: string = <string>op.sourceField

  if (typeof op.props.$list === 'object' && op.props.$list.$inherit) {
    const res = await executeNestedGetOperations(
      client,
      {
        $db: ctx.db,
        $id: op.id,
        result: {
          $field: op.sourceField,
          $inherit: op.props.$list.$inherit,
        },
      },
      lang,
      ctx,
      false,
      passedSchema
    )
    op.inKeys = res.result
  }

  const args = op.filter
    ? ast2rpn((passedSchema || client.schemas[ctx.db]).types, op.filter, lang)
    : ['#1']

  if (op.inKeys) {
    if (ctx.subId) {
      let added = false
      await Promise.all(
        op.inKeys.map(async (id) => {
          bufferFindMarker(ctx, {
            type: 'node',
            id: id,
            fields:
              op.props.$all === true
                ? []
                : [...fieldsOpt.get('$any').values()].filter(
                    (f) => !f.startsWith('!')
                  ),
            rpn: args,
          })
        })
      )
    }

    const result = await client.redis.selva_hierarchy_find(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      makeLangArg(client.schemas[ctx.db].languages, lang),
      '___selva_hierarchy',
      'node',
      'order',
      op.options.sort?.$field || '',
      op.options.sort?.$order || 'none',
      'offset',
      op.options.offset,
      'limit',
      op.options.limit,
      ...makeFieldsString(
        passedSchema || client.schemas[ctx.db],
        fieldsOpt,
        isInherit
      ),
      joinIds(op.inKeys),
      ...args
    )

    await checkForNextRefresh(
      ctx,
      client,
      sourceField,
      joinIds(op.inKeys),
      op.filter,
      lang,
      passedSchema
    )

    return result
  } else {
    const realOpts: any = {}
    for (const key in op.props) {
      if (!key.startsWith('$')) {
        realOpts[key] = true
      }
    }

    if (op.nested) {
      for (let i = 0; i < op.id.length; i += NODE_ID_SIZE) {
        let endLen = NODE_ID_SIZE
        while (op.id[i + endLen - 1] === '\0') {
          endLen--
        }
        const id = op.id.substr(i, endLen)
        // TODO: replace with slice scince substr is old
        // const id = op.id.slice(i, endLen + i)
        const schema = passedSchema || client.schemas[ctx.db]
        const sourceFieldSchema = getNestedSchema(schema, id, sourceField)

        bufferFindMarker(ctx, {
          ...sourceFieldToDir(
            schema,
            sourceFieldSchema,
            sourceField,
            op.recursive,
            op.byType
          ),
          id: id,
          fields: op.props.$all === true ? [] : Object.keys(realOpts),
          rpn: args,
        })

        await checkForNextRefresh(ctx, client, sourceField, id, op.filter, lang)
      }
    } else {
      const schema = passedSchema || client.schemas[ctx.db]
      const sourceFieldSchema = getNestedSchema(schema, op.id, sourceField)

      bufferFindMarker(ctx, {
        ...sourceFieldToDir(
          schema,
          sourceFieldSchema,
          sourceField,
          op.recursive,
          op.byType
        ),
        id: op.id,
        fields: op.props.$all === true ? [] : Object.keys(realOpts),
        rpn: args,
      })
    }

    const schema = passedSchema || client.schemas[ctx.db]
    const sourceFieldSchema = op.nested
      ? null
      : getNestedSchema(schema, op.id, sourceField)
    const result = await client.redis.selva_hierarchy_find(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      makeLangArg(client.schemas[ctx.db].languages, lang),
      '___selva_hierarchy',
      ...sourceFieldToFindArgs(
        schema,
        sourceFieldSchema,
        sourceField,
        op.recursive,
        op.byType
      ),
      ...mkIndex(schema, op),
      'order',
      op.options.sort?.$field || '',
      op.options.sort?.$order || 'none',
      'offset',
      op.options.offset,
      'limit',
      op.options.limit,
      ...makeFieldsString(schema, fieldsOpt, isInherit),
      padId(op.id),
      ...args
    )

    if (isInherit) {
      const findIds = result.map((el) => el[0])

      const inheritFields = new Set<string>()
      const inheritTypes = new Set<string>()
      const _ = [...fieldsOpt.get('$any')]
        .filter((f) => {
          return f.startsWith('^')
        })
        .map((f) => {
          const parts = f.split(':')
          const typePart = parts[0].slice(1)
          for (let i = 0; i < typePart.length; i += 2) {
            const pf = typePart[i] + typePart[i + 1]
            const type = schema.prefixToTypeMapping[pf]
            inheritTypes.add(type)
          }

          const inheritField = parts[1]
          inheritFields.add(inheritField)
        })

      const fork = {
        isFork: true,
        $or: [...inheritTypes].map((t) => {
          return {
            $field: 'type',
            $operator: '=',
            $value: t,
          }
        }),
      } as any

      const inheritRpn = ast2rpn(schema.types, fork, lang)

      for (const id of findIds) {
        bufferFindMarker(ctx, {
          type: 'ancestors',
          id: id,
          fields: [...inheritFields],
          rpn: inheritRpn,
        })
      }
    }

    await checkForNextRefresh(
      ctx,
      client,
      sourceField,
      padId(op.id),
      op.filter,
      lang
    )

    return result
  }
}

const executeFindOperation = async (
  client: SelvaClient,
  op: GetOperationFind,
  lang: string,
  ctx: ExecContext,
  passedSchema?: Schema
): Promise<GetResult> => {
  const schema = passedSchema || client.schemas[ctx.db]

  if (op.nested) {
    let ids = await findIds(client, op, lang, ctx, passedSchema)
    let nestedOperation = op.nested
    while (nestedOperation) {
      ids = await findIds(
        client,
        Object.assign({}, nestedOperation, {
          id: joinIds(ids),
        }),
        lang,
        ctx,
        passedSchema
      )
      nestedOperation = nestedOperation.nested
    }

    const realOpts: any = {}
    for (const key in op.props) {
      if (key === '$all' || !key.startsWith('$')) {
        realOpts[key] = op.props[key]
      }
    }

    const results = await Promise.all(
      ids.map(async (id) => {
        return await executeNestedGetOperations(
          client,
          {
            $db: ctx.db,
            $id: id,
            ...realOpts,
          },
          lang,
          ctx,
          false,
          passedSchema
        )
      })
    )

    if (op.single) {
      return results[0]
    }

    return results
  }

  const [fieldOpts, fieldMapping, additionalGets, isInherit] = parseGetOpts(
    schema,
    op.props,
    ''
  )

  if (additionalGets) {
    const ids = await findIds(client, op, lang, ctx, passedSchema)
    const allResults = []
    for (const id of ids) {
      const realOpts: any = {}
      for (const key in op.props) {
        if (['$all', '$fieldsByType'].includes(key) || !key.startsWith('$')) {
          realOpts[key] = op.props[key]
        }
      }

      const fieldResults = await executeNestedGetOperations(
        client,
        {
          $db: ctx.db,
          $id: id,
          ...realOpts,
        },
        lang,
        ctx,
        false,
        passedSchema
      )

      allResults.push(fieldResults)
    }

    if (op.single) {
      return allResults[0]
    }

    return allResults
  }

  const results: any[] = await findFields(
    client,
    op,
    lang,
    ctx,
    fieldOpts,
    isInherit,
    passedSchema
  )

  // **SubscriptionMarker mgmt**
  // Track ambiguous/implicit reference fields found in the query response that might need markers.
  // If `op` contains a `$find` then we are already tracking it elsewhere as we know there may
  // be references in the response.
  const ambiguousReferenceFields = op.props['$find']
    ? null
    : new Map<string, string[]>() // nodeId = [originField, ...fieldName(s)]

  const allMappings = new Set(Object.keys(fieldMapping))
  const result = []
  for (const entry of results) {
    const [id, fieldResults] = entry
    const entryRes: any = {}
    const usedMappings = new Set()
    for (let i = 0; i < fieldResults.length; i += 2) {
      const field = fieldResults[i]
      const value = fieldResults[i + 1]

      if (field === 'id') {
        entryRes.id = id
        continue
      }

      const isNestedObject = (v: any) => Array.isArray(v[0])
      const parseNestedFieldReference = (path: string, v: any[]) => {
        const nestedId = v[1]
        for (let i = 2; i < v.length; i += 2) {
          const nestedField = v[i]
          const nestedValue = v[i + 1]
          const isReferences = path.endsWith('[]')
          const fullPath = `${
            isReferences ? path.slice(0, -2) : path
          }.${nestedField}`

          const nestedFieldSchema = getNestedSchema(
            schema,
            nestedId,
            nestedField
          )
          if (
            nestedFieldSchema?.type === 'reference' &&
            isNestedObject(nestedValue)
          ) {
            parseNestedFieldReference(fullPath, nestedValue[0])
          } else if (
            nestedFieldSchema?.type === 'references' &&
            isNestedObject(nestedValue)
          ) {
            for (let i = 0; i < value.length; i++) {
              parseNestedFieldReference(`${fullPath}[]`, nestedValue[i])
            }
          } else {
            const mapping = fieldMapping[`${path}.${nestedField}`]
            const targetField = mapping?.targetField
            const casted = typeCast(
              nestedValue,
              nestedId,
              nestedField,
              schema,
              lang
            )

            if (
              ambiguousReferenceFields &&
              nestedId != id &&
              !ambiguousReferenceFields.get(id)
            ) {
              const destFields = op.props[field]

              if (destFields) {
                ambiguousReferenceFields.set(id, [
                  field,
                  ...Object.keys(destFields),
                ])
              }
            }

            if (targetField) {
              for (const f of targetField) {
                setNestedResult(entryRes, f, casted)
              }
            } else {
              if (isReferences) {
                setNestedResult(entryRes, path.slice(0, -2), [
                  { [nestedField]: casted },
                ])
              } else {
                setNestedResult(entryRes, fullPath, casted)
              }
            }
          }
        }
      }

      const sourceFieldSchema = getNestedSchema(schema, id, field)
      if (sourceFieldSchema?.type === 'reference' && isNestedObject(value)) {
        parseNestedFieldReference(field, value[0])
      } else if (
        sourceFieldSchema?.type === 'references' &&
        isNestedObject(value)
      ) {
        for (let i = 0; i < value.length; i++) {
          parseNestedFieldReference(`${field}[]`, value[i])
        }
      } else {
        const mapping = fieldMapping[field]
        const targetField = mapping?.targetField

        if (mapping?.isInherit) {
          const casted = typeCast(value[1], value[0], field, schema, lang)
          if (targetField) {
            for (const f of targetField) {
              setNestedResult(entryRes, f, casted)
            }
          } else {
            setNestedResult(entryRes, field, casted)
          }

          usedMappings.add(field)

          continue
        }

        const casted =
          id === EMPTY_ID
            ? typeCast(value, op.id, `${op.field}[0].${field}`, schema, lang)
            : typeCast(value, id, field, schema, lang)

        if (targetField) {
          for (const f of targetField) {
            setNestedResult(entryRes, f, casted)
          }
        } else {
          setNestedResult(entryRes, field, casted)
        }

        usedMappings.add(field)
      }
    }

    const unusedMappings = new Set(
      [...allMappings].filter((x) => !usedMappings.has(x))
    )

    for (const k of unusedMappings) {
      const mapping = fieldMapping[k]
      if (mapping?.default) {
        setNestedResult(entryRes, k, mapping.default)
      }
    }

    result.push(entryRes)
  }

  if (ambiguousReferenceFields) {
    for (const v of ambiguousReferenceFields) {
      const [id, [originField, ...fields]] = v

      bufferFindMarker(ctx, {
        type: 'edge_field',
        refField: originField,
        id,
        fields,
      })
    }
  }

  if (op.single) {
    return result[0]
  }

  return result
}

export default executeFindOperation
