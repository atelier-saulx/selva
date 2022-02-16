import { Schema, SchemaMutations } from '.'
import { SelvaClient } from '..'

import executeGetOperations from '../get/executeGetOperations'
import createGetOperations from '../get/createGetOperations'

export default async (
  db: string,
  client: SelvaClient,
  mutations: SchemaMutations,
  handleMutations: (old: { [field: string]: any }) => {
    [field: string]: any
  },
  oldSchema?: Schema
): Promise<void> => {
  console.info('????????????', mutations)
  const setObject: { [key: string]: any } = {}

  const gets = {}

  for (const f of mutations) {
    if (!gets[f.type]) {
      gets[f.type] = {
        id: true,
      }
    }

    let x = gets[f.type]
    for (let i = 0; i < f.path.length - 1; i++) {
      const p = f.path[i]
      if (!x[p]) {
        x[p] = {}
      }
      x = x[p]
    }
    x[f.path[f.path.length - 1]] = true
    // console.info(f.path)
  }

  for (const type in gets) {
    // delete if its a different field name...

    let page = 0

    // 5k

    // this get needs to be different....

    const query = {
      nodes: {
        ...gets[type],
        $list: {
          $offset: page * 5000,
          $limit: 5000,
          $find: {
            $traverse: 'descendants',
            $filter: {
              $operator: '=',
              $field: 'type',
              $value: type,
            },
          },
        },
      },
    }

    const op = createGetOperations(
      client,
      {
        ...gets[type],
        $list: {
          $offset: page * 5000,
          $limit: 5000,
          $find: {
            $traverse: 'descendants',
            $filter: {
              $operator: '=',
              $field: 'type',
              $value: type,
            },
          },
        },
      },
      'root',
      '.nodes',
      db,
      undefined,
      oldSchema
    )

    console.log(op)

    const r = await executeGetOperations(
      client,
      undefined,
      {
        db,
        meta: {},
        originDescriptors: {},
      },
      op,
      false,
      oldSchema
    )

    console.info('????????????', r)

    // get + old schema

    // need to call get with a different schema...

    // call get directly - need to be able to pass custom schema

    // oldSchema

    // const existing = await get(query)
    // console.dir(existing, { depth: 10 })
    // console.dir(query, { depth: 10 })
  }

  console.info(gets)

  // for (const mutation)
}
