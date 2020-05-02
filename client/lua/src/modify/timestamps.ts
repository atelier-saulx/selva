import * as redis from '../redis'
import * as logger from '../logger'
import { now } from '../util'
import { getSchema } from '../schema/index'
import sendEvent from './events'
import { getTypeFromId } from '../typeIdMapping'
import { SetOptions } from '~selva/set/types'

const UPDATED: Record<string, true> = {}

export function markUpdated(id: string): void {
  UPDATED[id] = true
}

export function setCreatedAt(id: string, type?: string): number | undefined {
  if (!type) {
    type = getTypeFromId(id)
  }

  const time = now()
  const schema = getSchema()

  const fields =
    type === 'root' ? schema.rootType.fields : schema.types[type].fields
  if (fields && fields.createdAt && fields.createdAt.type === 'timestamp') {
    redis.hset(id, 'createdAt', time)
    sendEvent(id, 'createdAt', 'update')
    return time
  }
}

export function setUpdatedAt(
  payload: SetOptions,
  id: string,
  type?: string,
  time?: number
) {
  if (payload.updatedAt) {
    return
  }

  if (!type) {
    type = getTypeFromId(id)
  }

  if (!UPDATED[id]) {
    return
  }

  const schema = getSchema()

  const fields =
    type === 'root' ? schema.rootType.fields : schema.types[type].fields

  if (fields && fields.updatedAt && fields.updatedAt.type === 'timestamp') {
    redis.hset(id, 'updatedAt', time || now())
    sendEvent(id, 'updatedAt', 'update')
  }
}
