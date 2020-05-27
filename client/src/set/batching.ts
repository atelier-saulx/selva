import { SelvaClient } from '..'
import { SetOptions, BatchOpts, BatchRefFieldOpts } from './types'
import { Schema } from '../schema'
import { _set } from '.'

export const MAX_BATCH_SIZE = 3500

export async function setInBatches(
  schemas: Schema,
  client: SelvaClient,
  payload: SetOptions | SetOptions[],
  depth: number,
  context?: {
    db?: string
    id?: string
    alias?: string | string[]
    field?: string
    add?: boolean
    $_batchOpts?: BatchOpts
  }
): Promise<string[]> {
  if (Array.isArray(payload)) {
    const { $_batchOpts, field, add, id } = context
    // context is always defined here
    // const BATCH_SIZE = MAX_BATCH_SIZE - 10 // allow for some operations to be batched with these
    const BATCH_SIZE = MAX_BATCH_SIZE
    const allIds: string[] = []
    let idx = 0

    do {
      const slice = payload.slice(idx, idx + BATCH_SIZE)

      const entries = await Promise.all(
        slice.map(async i => {
          if (typeof i === 'string' || i.$id || i.$alias) {
            return i
          }

          const id = await client.id({
            type: i.type
          })

          i.$id = id
          return i
        })
      )

      const refFieldOpts: BatchRefFieldOpts = {
        last: idx + BATCH_SIZE >= payload.length
      }

      if (!add) {
        refFieldOpts.resetReference = field
      }

      const opts: SetOptions =
        add || idx > 0
          ? {
              $_batchOpts: Object.assign({}, $_batchOpts, {
                refField: refFieldOpts
              }),
              $id: id,
              [field]: { $add: entries }
            }
          : {
              $_batchOpts: Object.assign({}, $_batchOpts, {
                refField: refFieldOpts
              }),
              $id: id,
              [field]: entries
            }

      await _set(client, opts, schemas.sha, context.db)

      const ids = entries.map(e => {
        if (typeof e === 'string') {
          return e
        }

        return e.$id
      })

      allIds.push(...(<string[]>ids))
      idx += BATCH_SIZE
    } while (idx < payload.length)

    return allIds
  }

  if (payload.$_itemCount < MAX_BATCH_SIZE && payload.$_itemCount !== 0) {
    const id = await _set(client, payload, schemas.sha, context.db)

    payload.$id = <string>id
    payload.$_itemCount = 0
    return [<string>id]
  }

  const db: string | undefined = payload.$db || context.db || 'default'
  const { $_batchOpts } = context
  const size = payload.$_itemCount
  let fieldNames: string[] = []
  let missingFieldNames: string[] = []
  let batchQueue: SetOptions[] = []
  let remainingBatchSize = MAX_BATCH_SIZE
  for (const field in payload) {
    if (payload[field].$_itemCount) {
      if (remainingBatchSize - payload[field].$_itemCount >= 0) {
        remainingBatchSize -= payload[field].$_itemCount
        batchQueue.push(payload[field])
        fieldNames.push(field)
      } else if (payload[field].$_itemCount > MAX_BATCH_SIZE) {
        if (!payload.$id && !payload.$alias) {
          payload.$id = await client.id({
            type: payload.type
          })
        }

        if (payload[field].$add) {
          await setInBatches(schemas, client, payload[field].$add, depth + 1, {
            db,
            id: payload.$id,
            alias: payload.$alias,
            field,
            add: true,
            $_batchOpts
          })
        } else if (Array.isArray(payload[field])) {
          await setInBatches(schemas, client, payload[field], depth + 1, {
            db,
            id: payload.$id,
            alias: payload.$alias,
            field,
            add: false,
            $_batchOpts
          })
        } else {
          fieldNames.push(field)
        }
      } else {
        missingFieldNames.push(field)
      }
    } else {
      fieldNames.push(field)
    }
  }

  const newPayload: SetOptions = { $_batchOpts }
  for (const field of fieldNames) {
    newPayload[field] = payload[field]
  }

  const missingPayload: SetOptions = {}
  let missingFieldsCount = 0
  for (const field of missingFieldNames) {
    missingFieldsCount++
    missingPayload[field] = payload[field]
  }

  const isLast = depth === 0 && missingFieldsCount === 0
  newPayload.$_batchOpts = {
    batchId: $_batchOpts.batchId,
    last: isLast
  }

  newPayload.$_itemCount = MAX_BATCH_SIZE - remainingBatchSize
  const id = await _set(client, newPayload, schemas.sha, db)

  if (missingFieldsCount) {
    missingPayload.$id = <string>id
    missingPayload.$_itemCount = size - newPayload.$_itemCount
    await setInBatches(schemas, client, missingPayload, depth, {
      $_batchOpts,
      db
    })
  }

  return [<string>id]
}
