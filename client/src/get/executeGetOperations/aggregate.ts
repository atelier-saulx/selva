import { Schema, SelvaClient } from '../../'
import { GetOperationAggregate, GetOperationFind } from '../types'
import { checkForNextRefresh, findIds } from './find'
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
  sourceFieldToFindArgs,
  bufferFindMarker,
} from './'
import { padId, joinIds, NODE_ID_SIZE } from '../../util'
import { getNestedSchema } from '../utils'
import { makeLangArg } from './util'
import { deepCopy } from '@saulx/utils'
import { mkIndex } from './indexing'

const FN_TO_ENUM = {
  count: '0',
  countUnique: '1',
  sum: '2',
  avg: '3',
  min: '4',
  max: '5',
}

// TODO: implement recursive version
const executeAggregateOperation = async (
  client: SelvaClient,
  op: GetOperationAggregate,
  lang: string,
  ctx: ExecContext,
  passedSchema?: Schema
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

    let ids = await findIds(client, findOp, lang, ctx, passedSchema)
    let nestedOperation = op.nested
    while (nestedOperation) {
      ids = await findIds(
        client,
        // TODO: needs fixing
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
      op.options.sort?.$order || 'none',
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

    const schema = client.schemas[ctx.db]
    const sourceFieldSchema = op.nested
      ? null
      : getNestedSchema(schema, op.id, sourceField)
    const agg = await client.redis.selva_hierarchy_aggregate(
      ctx.originDescriptors[ctx.db] || { name: ctx.db },
      makeLangArg(client.schemas[ctx.db].languages, lang),
      '___selva_hierarchy',
      FN_TO_ENUM[op.function.name] || '0',
      ...sourceFieldToFindArgs(
        client.schemas[ctx.db],
        sourceFieldSchema,
        sourceField,
        false,
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
