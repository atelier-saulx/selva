import { SelvaClient } from '../../'
import { GetOperationFind, GetResult } from '../types'
import { ast2rpn } from '@saulx/selva-query-ast-parser'
import { executeNestedGetOperations, ExecContext, addMarker } from './'
import { padId, joinIds } from '../utils'

const findHierarchy = async (
  client: SelvaClient,
  op: GetOperationFind,
  lang: string,
  ctx: ExecContext
): Promise<string[]> => {
  const { db, subId } = ctx

  let sourceField: string = <string>op.sourceField
  if (Array.isArray(op.sourceField)) {
    const exists = await Promise.all(
      op.sourceField.map(f => {
        if (
          ['children', 'parents', 'ancestors', 'descendants'].indexOf(f) !== -1
        ) {
          return true
        }
        return client.redis.hexists({ name: db }, op.id, f)
      })
    )
    const idx = exists.findIndex(x => !!x)
    if (idx === -1) {
      return
    }
    sourceField = op.sourceField[idx]
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
          $id: id,
          ...realOpts
        },
        lang,
        { db: ctx.db }
      )
    })
  )

  if (op.single) {
    return results[0]
  }

  return results
}

export default executeFindOperation
