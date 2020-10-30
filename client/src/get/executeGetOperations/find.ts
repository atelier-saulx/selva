import { SelvaClient } from '../../'
import { GetOperationFind, GetResult, GetOperation } from '../types'
import { ast2rpn, Fork, FilterAST, isFork } from '@saulx/selva-query-ast-parser'
import { executeNestedGetOperations, ExecContext, addMarker } from './'
import { padId, joinIds } from '../utils'

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
  const timebased = findTimebased(ast).filter(f => {
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
    timebased.map(async f => {
      const newFilter = Object.assign({}, f)
      newFilter.$operator = '>'

      let newFork: Fork = {
        isFork: true,
        $and: [withoutTimebased, newFilter]
      }

      if (!withoutTimebased) {
        newFork.$and = [newFilter]
      }

      const args = ast2rpn(newFork, lang)
      const ids = await client.redis.selva_hierarchy_find(
        {
          name: ctx.db
        },
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
        await client.redis.selva_object_get({ name: ctx.db }, id, f.$field)
      )

      if (!ctx.meta.___refreshAt || ctx.meta.___refreshAt > time) {
        ctx.meta.___refreshAt = time
      }
    })
  )
}

const findHierarchy = async (
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
          $inherit: op.props.$list.$inherit
        }
      },
      lang,
      ctx
    )

    op.inKeys = res.result
  } else if (Array.isArray(op.sourceField)) {
    sourceField = op.sourceField.join('\n')
  }
  const args = op.filter ? ast2rpn(op.filter, lang) : ['#1']
  // TODO: change this if ctx.subId (for markers)
  if (op.inKeys) {
    // can make this a bit better....
    const ids = await client.redis.selva_hierarchy_findin(
      {
        name: db
      },
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

    const added = await addMarker(client, ctx, {
      type: sourceField,
      id: op.id,
      fields: Object.keys(realOpts),
      rpn: args
    })

    if (added) {
      ctx.hasFindMarkers = true
    }

    const ids = await client.redis.selva_hierarchy_find(
      {
        name: db
      },
      '___selva_hierarchy',
      'bfs',
      sourceField,
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

const executeFindOperation = async (
  client: SelvaClient,
  op: GetOperationFind,
  lang: string,
  ctx: ExecContext
): Promise<GetResult> => {
  let ids = await findHierarchy(client, op, lang, ctx)

  if (op.nested) {
    let nestedOperation = op.nested
    let prevIds = ids
    while (nestedOperation) {
      ids = await findHierarchy(
        client,
        Object.assign({}, nestedOperation, {
          id: joinIds(ids)
        }),
        lang,
        ctx
      )
      prevIds = ids
      nestedOperation = nestedOperation.nested
    }
  }

  const realOpts: any = {}
  for (const key in op.props) {
    if (!key.startsWith('$')) {
      realOpts[key] = op.props[key]
    }
  }

  const results = await Promise.all(
    ids.map(async id => {
      return await executeNestedGetOperations(
        client,
        {
          $db: ctx.db,
          $id: id,
          ...realOpts
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

export default executeFindOperation
