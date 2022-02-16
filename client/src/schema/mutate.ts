import { Schema, SchemaMutations } from '.'
import { SelvaClient } from '..'

import executeGetOperations from '../get/executeGetOperations'
import createGetOperations from '../get/createGetOperations'

const pageAmount = 1e3

export default async (
  db: string,
  client: SelvaClient,
  mutations: SchemaMutations,
  handleMutations: (old: { [field: string]: any }) => {
    [field: string]: any
  },
  oldSchema?: Schema
): Promise<void> => {
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

    let finished = false

    while (!finished) {
      const op = createGetOperations(
        client,
        {
          ...gets[type],
          $list: {
            $offset: page * pageAmount,
            $limit: pageAmount,
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

      const setQ = []

      for (const node of r.nodes) {
        // if false remove all mutations
        setQ.push(
          client.set({ $id: node.id, ...handleMutations(node), $db: db })
        )
      }
      await Promise.all(setQ)

      console.info(`Set mutation batch #${page} ${page * pageAmount}`)

      if (r.nodes.length === pageAmount) {
        page++
      } else {
        finished = true
        break
      }
    }
  }

  console.info('RDY!')

  // for (const mutation)
}
