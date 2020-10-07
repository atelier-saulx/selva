import { SelvaClient } from '../../'
import { GetOperation, GetOperationFind, GetResult } from '../types'
import { ast2rpn } from '@saulx/selva-query-ast-parser'
import { executeNestedGetOperations } from './'
import { padId } from '../utils'

const findHierarchy = async (
  client: SelvaClient,
  op: GetOperationFind,
  lang: string,
  db: string
): Promise<string[]> => {
  let sourceField: string = <string>op.sourceField
  if (Array.isArray(op.sourceField)) {
    const exists = await Promise.all(
      op.sourceField.map(f => {
        if (
          ['children', 'parents', 'ancestors', 'descendants'].indexOf(f) !== -1
        ) {
          return true
        }
        return client.redis.hexists(op.id, f)
      })
    )
    const idx = exists.findIndex(x => !!x)
    if (idx === -1) {
      return
    }
    sourceField = op.sourceField[idx]
  }
  const args = op.filter ? ast2rpn(op.filter, lang) : ['#1']
  if (op.inKeys) {
    console.log('MAKE INKEYS', op.inKeys)
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
      op.id,
      ...args
    )
  } else {
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
      op.id,
      ...args
    )
    return ids
  }
}

const executeFindOperation = async (
  client: SelvaClient,
  op: GetOperationFind,
  lang: string,
  db: string
): Promise<GetResult> => {
  let ids = await findHierarchy(client, op, lang, db)

  if (op.nested) {
    let nestedOperation = op.nested
    let prevIds = ids
    while (nestedOperation) {
      ids = await findHierarchy(
        client,
        Object.assign({}, nestedOperation, {
          id: ids.map(id => padId(id)).join('')
        }),
        lang,
        db
      )
      prevIds = ids
      nestedOperation = nestedOperation.nested
    }
  }

  const results = await Promise.all(
    ids.map(async id => {
      const realOpts: any = {}
      for (const key in op.props) {
        if (!key.startsWith('$')) {
          realOpts[key] = op.props[key]
        }
      }
      return await executeNestedGetOperations(
        client,
        {
          $id: id,
          ...realOpts
        },
        lang,
        db
      )
    })
  )

  if (op.single) {
    return results[0]
  }

  return results
}

export default executeFindOperation
