import { Schema, SchemaMutations } from '.'
import { SelvaClient } from '..'

import executeGetOperations from '../get/executeGetOperations'
import createGetOperations from '../get/createGetOperations'

const pageAmount = 3e3

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
    if (f.mutation === 'change_field' || f.mutation === 'remove_field') {
      if (!gets[f.type]) {
        gets[f.type] = {
          query: {
            id: true,
          },
          setOp: null,
        }
      }

      if (f.mutation === 'remove_field') {
        if (!gets[f.type].setOp) {
          gets[f.type].setOp = {}
        }

        let x = gets[f.type].query
        let y = gets[f.type].setOp

        for (let i = 0; i < f.path.length - 1; i++) {
          const p = f.path[i]
          if (!x[p]) {
            x[p] = {}
          }
          if (!y[p]) {
            y[p] = {}
          }
          y = y[p]
          x = x[p]
        }
        y[f.path[f.path.length - 1]] = { $delete: true }
        x[f.path[f.path.length - 1]] = true
      } else {
        let x = gets[f.type].query
        for (let i = 0; i < f.path.length - 1; i++) {
          const p = f.path[i]
          if (!x[p]) {
            x[p] = {}
          }
          x = x[p]
        }
        x[f.path[f.path.length - 1]] = true
      }
    }
  }

  for (const type in gets) {
    // delete if its a different field name...
    let page = 0

    let finished = false

    while (!finished) {
      const op = createGetOperations(
        client,
        {
          ...gets[type].query,
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
        if (gets[type].setOp) {
          setQ.push(
            client.set(
              {
                $id: node.id,
                ...gets[type].setOp,
                $db: db,
              },
              oldSchema
            )
          )
        }

        setQ.push(
          client.set({
            $id: node.id,
            ...handleMutations(node),
            $db: db,
          })
        )
      }
      await Promise.all(setQ)

      // progress listener maybe?
      console.info(`Set mutation batch #${page} ${page * pageAmount}`)

      if (r.nodes.length === pageAmount) {
        try {
          if (global.gc) {
            global.gc()
          }
        } catch (err) {
          console.error(`Cannot manualy gc`, err)
        }
        page++
      } else {
        finished = true
        break
      }
    }
  }
}