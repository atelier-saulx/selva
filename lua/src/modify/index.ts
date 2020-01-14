import * as redis from '../redis'
import { Id } from '~selva/schema'
import { SetOptions } from '~selva/setTypes'
import getTypeFromId from '../getTypeFromId'
import { ensureArray } from '../util'
import resetSet from './resetSet'

// TODO: maintain parent/child relationships refactor
function setInternalArrayStructure(id: string, field: string, value: any) {
  if (Array.isArray(value)) {
    resetSet(id, field, value)
  } else {
    const hierarchy = value.$hierarchy === false ? false : true
    if (value.$value) {
      resetSet(id, field, value)
    }
    if (value.$add) {
      value.$add = ensureArray(value.$add)
      // TODO:
      // addToSet(field, hierarchy, id, value.$add)
    }
    if (value.$delete) {
      value.$delete = ensureArray(value.$delete)
      // TODO:
      // removeFromSet(field, hierarchy, id, value.$delete)
    }
  }
}

function setField(
  id: string,
  field: string,
  value: any,
  fromDefault: boolean
): void {
  if (!fromDefault && value.$merge === false) {
    const keys = redis.hkeys(id)
    // TODO: removeFields
  }

  if (field === 'parents') {
  } else if (field === 'children') {
  } else if (field === 'externalId' || field === 'auth.role.id') {
    // TODO: this
  }
}

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
