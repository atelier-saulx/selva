import { Schema, SchemaMutations } from '.'
import { SelvaClient } from '..'

import executeGetOperations from '../get/executeGetOperations'
import createGetOperations from '../get/createGetOperations'

const pageAmount = 3e3

export default async (
  db: string,
  client: SelvaClient,
  mutations: SchemaMutations,
  handleMutations: (old: { [field: string]: any }) => null | {
    [field: string]: any
  },
  oldSchema?: Schema
): Promise<void> => {
  const parsedFieldMutations: {
    [type: string]: { query: any; setOp: any; nullSetOp: any }
  } = {}

  for (const f of mutations) {
    if (f.mutation === 'change_field' || f.mutation === 'remove_field') {
      if (!parsedFieldMutations[f.type]) {
        parsedFieldMutations[f.type] = {
          query: {
            id: true,
          },
          setOp: null,
          nullSetOp: {
            id: true,
          },
        }
      }

      if (f.mutation === 'remove_field') {
        if (!parsedFieldMutations[f.type].setOp) {
          parsedFieldMutations[f.type].setOp = {}
        }
        let x = parsedFieldMutations[f.type].query
        let y = parsedFieldMutations[f.type].setOp
        let z = parsedFieldMutations[f.type].nullSetOp
        for (let i = 0; i < f.path.length - 1; i++) {
          const p = f.path[i]
          if (!x[p]) {
            x[p] = {}
          }
          if (!y[p]) {
            y[p] = {}
          }
          if (!z[p]) {
            z[p] = {}
          }
          y = y[p]
          x = x[p]
          z = z[p]
        }
        y[f.path[f.path.length - 1]] = { $delete: true }
        z[z.path[z.path.length - 1]] = { $delete: true }
        x[f.path[f.path.length - 1]] = true
      } else if (f.mutation === 'change_field') {
        let x = parsedFieldMutations[f.type].query
        let z = parsedFieldMutations[f.type].nullSetOp
        for (let i = 0; i < f.path.length - 1; i++) {
          const p = f.path[i]
          if (!x[p]) {
            x[p] = {}
          }
          if (!z[p]) {
            z[p] = {}
          }
          x = x[p]
          z = z[p]
        }
        x[f.path[f.path.length - 1]] = true
        z[z.path[z.path.length - 1]] = { $delete: true }
      }
    }
  }

  for (const type in parsedFieldMutations) {
    // delete if its a different field name...
    let page = 0

    let finished = false

    while (!finished) {
      const op = createGetOperations(
        client,
        {
          ...parsedFieldMutations[type].query,
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
        if (parsedFieldMutations[type].setOp) {
          setQ.push(
            client.set(
              {
                $id: node.id,
                ...parsedFieldMutations[type].setOp,
                $db: db,
              },
              oldSchema
            )
          )
        }

        const result = handleMutations(node)

        if (!result) {
          setQ.push(
            client.set({
              $id: node.id,
              ...parsedFieldMutations[type].nullSetOp,
              $db: db,
            })
          )
        } else {
          setQ.push(
            client.set({
              $id: node.id,
              ...result,
              $db: db,
            })
          )
        }
      }

      await Promise.all(setQ)

      // progress listener maybe? and remove this log
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
