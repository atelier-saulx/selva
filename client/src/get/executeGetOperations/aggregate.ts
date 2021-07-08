import { SelvaClient } from '../../'
import {
  GetOperationAggregate,
  GetResult,
  GetOptions,
  GetOperationFind,
} from '../types'
import { findIds } from './find'
import { typeCast } from './'
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
  sourceFieldToMarkerType,
  addMarker,
} from './'
import { padId, joinIds } from '../utils'
import { setNestedResult } from '../utils'
import { makeLangArg } from './util'
import { deepCopy } from '@saulx/utils'

const FN_TO_ENUM = {
  count: '0',
  countUnique: '1',
  sum: '2',
  avg: '3',
  min: '4',
  max: '5',
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
      // TODO
      const ids = await client.redis.selva_hierarchy_find(
        ctx.originDescriptors[ctx.db] || { name: ctx.db },
        makeLangArg(client.schemas[ctx.db].languages, lang),
        '___selva_hierarchy',
        'bfs',
        sourceField,
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

// TODO: implement recursive version
const executeAggregateOperation = async (
  client: SelvaClient,
  op: GetOperationAggregate,
  lang: string,
  ctx: ExecContext
): Promise<number> => {
  if (op.nested) {
    const findProps: any = deepCopy(op.props)
    findProps.$find = op.props.$aggregate
    delete findProps.$aggregate

    const findOp: GetOperationFind = {
      type: 'find',
      id: op.id,
      field: op.field,
      sourceField: op.sourceField,
      props: findProps,
      single: false,
      filter: op.filter,
      nested: op.nested,
      recursive: op.recursive,
      options: op.options,
    }

    let ids = await findIds(client, findOp, lang, ctx)
    let nestedOperation = op.nested
    let prevIds = ids
    while (nestedOperation) {
      ids = await findIds(
        client,
        // TODO: needs fixing
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

    return executeAggregateOperation(
      client,
      {
        type: 'aggregate',
        id: op.id,
        field: op.field,
        sourceField: op.sourceField,
        function: op.function,
        props: op.props,
        inKeys: ids,
        options: op.options,
      },
      lang,
      ctx
    )
  }

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
  } else if (Array.isArray(op.sourceField)) {
    sourceField = op.sourceField.join('\n')
  }
  const args = op.filter
    ? ast2rpn(client.schemas[ctx.db].types, op.filter, lang)
    : ['#1']
  // TODO: change this if ctx.subId (for markers)
  if (op.inKeys) {
    // can make this a bit better....
    // TODO? need this?
    const agg = await client.redis.selva_hierarchy_aggregatein(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      makeLangArg(client.schemas[ctx.db].languages, lang),
      '___selva_hierarchy',
      FN_TO_ENUM[op.function.name] || '0',
      'order',
      op.options.sort?.$field || '',
      op.options.sort?.$order || 'asc',
      'offset',
      op.options.offset,
      'limit',
      op.options.limit,
      'fields',
      (op.function.args || []).join('|'),
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

    return Number(agg)
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
        const r = await addMarker(client, ctx, {
          ...sourceFieldToMarkerType(sourceField),
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
      const added = await addMarker(client, ctx, {
        ...sourceFieldToMarkerType(sourceField),
        id: op.id,
        fields: op.props.$all === true ? [] : Object.keys(realOpts),
        rpn: args,
      })

      if (added) {
        ctx.hasFindMarkers = true
      }
    }

    const agg = await client.redis.selva_hierarchy_aggregate(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      makeLangArg(client.schemas[ctx.db].languages, lang),
      '___selva_hierarchy',
      FN_TO_ENUM[op.function.name] || '0',
      sourceField,
      'order',
      op.options.sort?.$field || '',
      op.options.sort?.$order || 'asc',
      'offset',
      op.options.offset,
      'limit',
      op.options.limit,
      'fields',
      (op.function.args || []).join('|'),
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

    return Number(agg)
  }
}

export default executeAggregateOperation
