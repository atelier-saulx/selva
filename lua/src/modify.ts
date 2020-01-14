import * as redis from './redis'
import { Id } from 'selva/schema'
import { SetOptions } from 'selva/set'
import getTypeFromId from './getTypeFromId'

// We always set an $id property before passing to redis
export default function modify(payload: SetOptions & { $id: string }): Id {
  const exists = !!payload.$id ? redis.hexists(payload.$id, 'type') : false

  if (!exists) {
    if (!payload.type) {
      payload.type = getTypeFromId(payload.$id)
    }

    if (!payload.parents && payload.$id !== 'root') {
      payload.parents = { $add: 'root' }
    }
  }

  // TODO: await setInner
  return payload.$id
}
