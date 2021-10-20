import { SelvaClient } from '../../'
import { GetOperationFind, GetResult, GetOptions } from '../types'
import { sourceFieldToFindArgs, typeCast } from './'
import {
  ast2rpn,
  Fork,
  FilterAST,
  isFork,
  convertNow,
} from '@saulx/selva-query-ast-parser'
import {
  executeNestedGetOperations,
  ExecContext,
  sourceFieldToDir,
  addMarker,
} from './'
import { padId, joinIds, EMPTY_ID } from '../utils'
import { setNestedResult, getNestedSchema } from '../utils'
import { makeLangArg } from './util'
import { mkIndex } from './indexing'

function makeFieldsString(fields: Set<string>): string {
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

  return str
}

function parseGetOpts(
  props: GetOptions,
  path: string,
  nestedMapping?: Record<string, { targetField?: string[]; default?: any }>
): [
  Set<string>,
  Record<string, { targetField?: string[]; default?: any }>,
  boolean
] {
  const pathPrefix = path === '' ? '' : path + '.'
  let fields: Set<string> = new Set()
  const mapping: Record<
    string,
    {
      targetField?: string[]
      default?: any
    }
  > = nestedMapping || {}

  let hasAll = false

  for (const k in props) {
    if ((k === '$list' || k === '$find') && pathPrefix === '') {
      // ignore
    } else if (props.$list && k === '$field' && pathPrefix === '') {
      // ignore
    } else if (!hasAll && !k.startsWith('$') && props[k] === true) {
      fields.add(pathPrefix + k)
    } else if (props[k] === false) {
      fields.add(`!${pathPrefix + k}`)
    } else if (k === '$field') {
      const $field = props[k]
      if (Array.isArray($field)) {
        fields.add($field.join('|'))
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
        fields.add($field)

        if (!mapping[$field]) {
          mapping[$field] = { targetField: [path] }
        } else if (!mapping[$field].targetField) {
          mapping[$field].targetField = [path]
        } else {
          mapping[$field].targetField.push(path)
        }
      }
    } else if (k === '$default') {
      fields.add(path)

      const $default = props[k]
      if (!mapping[path]) {
        mapping[path] = { default: $default }
      } else {
        mapping[path].default = $default
      }
    } else if (path === '' && k === '$all') {
      // fields = new Set(['*'])
      fields.add('*')
      // hasAll = true
    } else if (k === '$all') {
      fields.add(path + '.*')
    } else if (k.startsWith('$')) {
      return [fields, mapping, true]
    } else if (typeof props[k] === 'object') {
      const [nestedFields, , hasSpecial] = parseGetOpts(
        props[k],
        pathPrefix + k,
        mapping
      )

      if (hasSpecial) {
        return [fields, mapping, true]
      }

      for (const f of nestedFields.values()) {
        fields.add(f)
      }
    }
  }

  return [fields, mapping, false]
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
  lang?: string
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

      let newFork: Fork = {
        isFork: true,
        $and: [withoutTimebased, newFilter],
      }

      if (!withoutTimebased) {
        newFork.$and = [newFilter]
      }

      const args = ast2rpn(client.schemas[ctx.db].types, newFork, lang)
      let ids = await client.redis.selva_hierarchy_find(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        makeLangArg(client.schemas[ctx.db].languages, lang),
        '___selva_hierarchy',
        // TODO: needs byType expression
        ...sourceFieldToFindArgs(
          client.schemas[ctx.db],
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
        await client.redis.selva_object_get(
          ctx.originDescriptors[ctx.db] || { name: ctx.db },
          makeLangArg(client.schemas[ctx.db].languages, lang),
          id,
          f.$field
        )
      )

      let v = <string>f.$value
      if (v.startsWith('now-')) {
        v = v.replace('now-', 'now+')
      } else if (v.startsWith('now+')) {
        v = v.replace('now+', 'now-')
      }

      let converted = convertNow(v, time)
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
  ctx: ExecContext
): Promise<string[]> => {
  const { db, subId } = ctx

  let sourceField: string = <string>op.sourceField
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
      ctx
    )

    op.inKeys = res.result
  }

  const args = op.filter
    ? ast2rpn(client.schemas[ctx.db].types, op.filter, lang)
    : ['#1']
  // TODO: change this if ctx.subId (for markers)
  if (op.inKeys) {
    // can make this a bit better....
    const ids = await client.redis.selva_hierarchy_findin(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      lang,
      '___selva_hierarchy',
      'order',
      op.options.sort?.$field || '',
      op.options.sort?.$order || 'asc',
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
      for (let i = 0; i < op.id.length; i += 10) {
        let endLen = 10
        while (op.id[i + endLen - 1] === '\0') {
          endLen--
        }
        const id = op.id.slice(i, endLen)
        const schema = client.schemas[ctx.db]
        const sourceFieldSchema = getNestedSchema(schema, id, sourceField)
        const r = await addMarker(client, ctx, {
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

        added = added || r

        await checkForNextRefresh(ctx, client, sourceField, id, op.filter, lang)
      }

      if (added) {
        ctx.hasFindMarkers = true
      }
    } else {
      const schema = client.schemas[ctx.db]
      const sourceFieldSchema = getNestedSchema(schema, op.id, sourceField)
      const added = await addMarker(client, ctx, {
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

      if (added) {
        ctx.hasFindMarkers = true
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
      op.options.sort?.$order || 'asc',
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
  fieldsOpt
): Promise<string[]> => {
  const { db, subId } = ctx

  let sourceField: string = <string>op.sourceField
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
      ctx
    )

    op.inKeys = res.result
  }

  const args = op.filter
    ? ast2rpn(client.schemas[ctx.db].types, op.filter, lang)
    : ['#1']
  // console.log('ARGS', args)
  if (op.inKeys) {
    if (ctx.subId) {
      let added = false
      await Promise.all(
        op.inKeys.map(async (id) => {
          const r = await addMarker(client, ctx, {
            type: 'node',
            id: id,
            fields:
              op.props.$all === true
                ? []
                : [...fieldsOpt.values()].filter((f) => !f.startsWith('!')),
            rpn: args,
          })

          added = added || r
        })
      )

      if (added) {
        ctx.hasFindMarkers = true
      }
    }

    const result = await client.redis.selva_hierarchy_findin(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      lang,
      '___selva_hierarchy',
      'order',
      op.options.sort?.$field || '',
      op.options.sort?.$order || 'asc',
      'offset',
      op.options.offset,
      'limit',
      op.options.limit,
      'fields',
      makeFieldsString(fieldsOpt),
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

    return result
  } else {
    const realOpts: any = {}
    for (const key in op.props) {
      if (!key.startsWith('$')) {
        realOpts[key] = true
      }
    }

    if (op.nested) {
      let added = false
      for (let i = 0; i < op.id.length; i += 10) {
        let endLen = 10
        while (op.id[i + endLen - 1] === '\0') {
          endLen--
        }
        const id = op.id.slice(i, endLen)
        const schema = client.schemas[ctx.db]
        const sourceFieldSchema = getNestedSchema(schema, id, sourceField)

        const r = await addMarker(client, ctx, {
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

        added = added || r

        await checkForNextRefresh(ctx, client, sourceField, id, op.filter, lang)
      }

      if (added) {
        ctx.hasFindMarkers = true
      }
    } else {
      const schema = client.schemas[ctx.db]
      const sourceFieldSchema = getNestedSchema(schema, op.id, sourceField)
      const added = await addMarker(client, ctx, {
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

      if (added) {
        ctx.hasFindMarkers = true
      }
    }

    const schema = client.schemas[ctx.db]
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
      op.options.sort?.$order || 'asc',
      'offset',
      op.options.offset,
      'limit',
      op.options.limit,
      'fields',
      makeFieldsString(fieldsOpt),
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

    return result
  }
}

const executeFindOperation = async (
  client: SelvaClient,
  op: GetOperationFind,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> => {
  const schema = client.schemas[ctx.db]

  if (op.nested) {
    let ids = await findIds(client, op, lang, ctx)
    let nestedOperation = op.nested
    let prevIds = ids
    while (nestedOperation) {
      ids = await findIds(
        client,
        Object.assign({}, nestedOperation, {
          id: joinIds(ids),
        }),
        lang,
        ctx
      )
      prevIds = ids

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
          ctx
        )
      })
    )

    if (op.single) {
      return results[0]
    }

    return results
  }

  const [fieldOpts, fieldMapping, additionalGets] = parseGetOpts(op.props, '')

  if (additionalGets) {
    let ids = await findIds(client, op, lang, ctx)
    const allResults = []
    for (const id of ids) {
      const realOpts: any = {}
      for (const key in op.props) {
        if (key === '$all' || !key.startsWith('$')) {
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
        ctx
      )

      allResults.push(fieldResults)
    }

    if (op.single) {
      return allResults[0]
    }

    return allResults
  }

  let results: any[] = await findFields(client, op, lang, ctx, fieldOpts)

  const allMappings = new Set(Object.keys(fieldMapping))
  const result = []
  for (let entry of results) {
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

      const mapping = fieldMapping[field]
      const targetField = mapping?.targetField
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

  if (op.single) {
    return result[0]
  }

  return result
}

export default executeFindOperation
