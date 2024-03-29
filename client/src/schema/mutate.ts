import { Schema, SchemaMutations } from '.'
import { SelvaClient } from '..'

import executeGetOperations from '../get/executeGetOperations'
import createGetOperations from '../get/createGetOperations'

const pageAmount = 3e3

export default async (
  db: string,
  client: SelvaClient,
  mutations: SchemaMutations,
  handleMutations?: (old: { [field: string]: any }) => null | {
    [field: string]: any
  },
  oldSchema?: Schema
): Promise<void> => {
  const parsedFieldMutations: {
    [type: string]: { query: any; setOp: any; nullSetOp: any }
  } = {}

  for (const f of mutations) {
    if (f.mutation === 'delete_type') {
      let page = 0
      let finished = false
      while (!finished) {
        const fields: any = {}

        if (!handleMutations) {
          fields.id = true
        } else {
          for (const field in oldSchema.types[f.type].fields) {
            if (field !== 'ancestors' && field !== 'descendants') {
              fields[field] = true
            }
          }
        }

        const op = createGetOperations(
          client,
          {
            ...fields,
            $list: {
              $offset: page * pageAmount,
              $limit: pageAmount,
              $find: {
                $traverse: 'descendants',
                $filter: {
                  $operator: '=',
                  $field: 'type',
                  $value: f.type,
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
        const setQRemovals = []

        for (const node of r.nodes) {
          setQRemovals.push(client.delete({ $id: node.id }))
          const result = handleMutations ? handleMutations(node) : null
          if (result) {
            setQ.push(client.set(result))
          }
        }

        await Promise.all(setQRemovals)
        await Promise.all(setQ)

        // progress listener maybe? and remove this log
        console.info(
          `Set type delete mutation batch #${page} ${page * pageAmount}`
        )

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

      // await
    } else if (f.mutation === 'change_field' || f.mutation === 'remove_field') {
      if (!parsedFieldMutations[f.type]) {
        parsedFieldMutations[f.type] = {
          query: {
            id: true,
          },
          setOp: null,
          nullSetOp: null,
        }
      }

      if (handleMutations) {
        let x = parsedFieldMutations[f.type].query
        for (let i = 0; i < f.path.length - 1; i++) {
          const p = f.path[i]
          if (!x[p]) {
            x[p] = {}
          }
          x = x[p]
        }
        x[f.path[f.path.length - 1]] = true
      }

      if (f.mutation === 'remove_field') {
        if (!parsedFieldMutations[f.type].setOp) {
          parsedFieldMutations[f.type].setOp = {}
        }
        let y = parsedFieldMutations[f.type].setOp
        for (let i = 0; i < f.path.length - 1; i++) {
          const p = f.path[i]
          if (!y[p]) {
            y[p] = {}
          }
          y = y[p]
        }
        y[f.path[f.path.length - 1]] = { $delete: true }
      } else if (f.mutation === 'change_field') {
        if (!parsedFieldMutations[f.type].nullSetOp) {
          parsedFieldMutations[f.type].nullSetOp = {}
        }
        let z = parsedFieldMutations[f.type].nullSetOp
        for (let i = 0; i < f.path.length - 1; i++) {
          const p = f.path[i]
          if (!z[p]) {
            z[p] = {}
          }
          z = z[p]
        }
        z[f.path[f.path.length - 1]] = { $delete: true }
      }
    }
  }
  for (const type in parsedFieldMutations) {
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
      const setQRemovals = []
      for (const node of r.nodes) {
        if (parsedFieldMutations[type].setOp) {
          setQRemovals.push(
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
        const result = handleMutations ? handleMutations(node) : null
        if (!result) {
          if (parsedFieldMutations[type].nullSetOp) {
            setQRemovals.push(
              client.set(
                {
                  $id: node.id,
                  ...parsedFieldMutations[type].nullSetOp,
                  $db: db,
                },
                oldSchema
              )
            )
          }
        } else if (result.type || (result.$id && result.$id !== node.id)) {
          if (parsedFieldMutations[type].nullSetOp) {
            setQRemovals.push(
              client.set(
                {
                  $id: node.id,
                  ...parsedFieldMutations[type].nullSetOp,
                  $db: db,
                },
                oldSchema
              )
            )
          }
          setQ.push(
            client.set({
              ...result,
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

      await Promise.all(setQRemovals)
      // maybe first
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
