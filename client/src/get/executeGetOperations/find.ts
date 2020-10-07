import { SelvaClient } from '../../'
import { GetOperation, GetResult } from '../types'
import { setNestedResult, getNestedSchema } from '../utils'
import resolveId from '../resolveId'
import createGetOperations from '../createGetOperations'
import { GetOptions } from '../'
import { ast2rpn } from '@saulx/selva-query-ast-parser'
import { executeNestedGetOperations } from './'
import { ServerSelector } from '~selva/types'

// make nice here
const executeFindOperation = async (
  client: SelvaClient,
  op: GetOptions,
  lang: string,
  db: string // this needs to go everywhere!
): Promise<GetResult> => {
  // everything needs this
  const selector: ServerSelector = {
    name: db
  }

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

  // todo IN KEYS

  if (op.inKeys) {
    console.log('MAKE INKEYS')
  }

  const args = op.filter ? ast2rpn(op.filter, lang) : ['#1']
  console.log('SORT', op.options?.sort, args, op.filter)

  // schmall opt dont onclude id in get options if its there

  let ids = await client.redis.selva_hierarchy_find(
    selector,
    '___selva_hierarchy',
    'bfs',
    sourceField,
    'order',
    op.options?.sort?.$field || '',
    op.options?.sort?.$order || 'asc',
    'offset',
    op.options.offset,
    'limit',
    op.options.limit,
    op.id,
    ...args
  )

  if (op.nested) {
    const makeNestedNice = async (ids: string[], nested: any) => {
      const makeOp = Object.assign({}, nested, {
        id: ids.map(id => id.padEnd(10, '\0')).join('')
      })

      let nids = await client.redis.selva_hierarchy_find(
        selector,
        '___selva_hierarchy',
        'bfs',
        makeOp.sourceField,
        'order',
        makeOp.options?.sort?.$field || '',
        makeOp.options?.sort?.$order || 'asc',
        'offset',
        0,
        'limit',
        -1,
        makeOp.id,
        ...(makeOp.filter ? ast2rpn(makeOp.filter, lang) : ['#1'])
      )
      return nids
    }

    let isNest = op.nested
    let level = 0
    let prevIds = ids
    while (isNest) {
      let nids = []
      // console.log('LEVEL ', level, prevIds.length, prevIds)

      nids = await makeNestedNice(prevIds, isNest)
      prevIds = nids

      level++
      isNest = isNest.nested
    }

    const results = await Promise.all(
      prevIds.map(id => {
        const realOpts: any = {}
        for (const key in op.props) {
          if (!key.startsWith('$')) {
            realOpts[key] = op.props[key]
          }
        }
        return executeNestedGetOperations(
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

  const results = await Promise.all(
    ids.map(async id => {
      const realOpts: any = {}
      for (const key in op.props) {
        if (!key.startsWith('$')) {
          realOpts[key] = op.props[key]
        }
      }

      const x = await executeNestedGetOperations(
        client,
        {
          $id: id,
          ...realOpts
        },
        lang,
        db
      )

      return x
    })
  )

  if (op.single) {
    return results[0]
  }

  return results
}

export default executeFindOperation
