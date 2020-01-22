import { id as genId } from '../id'
import * as redis from '../redis'
import { Id } from '~selva/schema/index'
import { SetOptions } from '~selva/set/types'
import { getTypeFromId } from '../typeIdMapping'
import { isString, isArray, splitString, joinString } from '../util'
import { resetSet, addToSet, removeFromSet } from './setOperations'
import { ModifyOptions, ModifyResult } from '~selva/modifyTypes'
import { DeleteOptions } from '~selva/delete/types'
import { deleteItem } from './delete'
import * as logger from '../logger'

function removeFields(
  id: string,
  field: string | null,
  value: object,
  keys: string[]
): void {
  const path = field ? splitString(field, '.') : []
  if (!field) {
    // no field is slightly different
    for (let key in value) {
      if (key[0] !== '$' && type(value[key]) === 'table') {
        removeFields(id, key, value[key], keys)
      }
    }
    return
  }

  for (let key in value) {
    if (key[0] !== '$') {
      for (const fieldKey of keys) {
        const fields = splitString(fieldKey, '.')
        let removeField = true
        for (let i = 0; i < path.length; i++) {
          if (fields[i] !== path[i]) {
            removeField = false
            break
          }
        }

        if (removeField) {
          removeField = false
          let segment = value
          for (let i = path.length; i < fields.length; i++) {
            segment = segment[fields[i]]
            if (!segment) {
              removeField = true
              break
            }
          }
          if (removeField) {
            redis.hdel(id, fieldKey)
          }
        }
      }
      if (type(value[key]) === 'table') {
        removeFields(id, joinString(path, '.'), value[key], keys)
      }
    }
  }
}

function setInternalArrayStructure(
  id: string,
  field: string,
  value: any
): void {
  const hierarchy = value.$hierarchy === false ? false : true

  if (isArray(value)) {
    resetSet(id, field, value, update, hierarchy)
    return
  }

  if (value.$value) {
    resetSet(id, field, value, update, hierarchy)
    return
  }

  if (value.$add) {
    addToSet(id, field, value.$add, update, hierarchy)
  }
  if (value.$delete) {
    removeFromSet(id, field, value.$delete, hierarchy)
  }
}

function setObject(id: string, field: string, item: any) {
  if (item.$value !== null) {
    setField(id, field, item, false)
  } else if (item.$default) {
    if (item.$increment) {
      const result = redis.hsetnx(id, field, item.$default)
      if (result === 0) {
        redis.hincrby(id, field, item.$increment)
      }
      return
    }

    setField(id, field, item.$default, true)
  } else if (item.$increment) {
    redis.hincrby(id, field, item.$increment)
  } else {
    setField(id, field, item, false)
  }
}

function setField(
  id: string,
  field: string | null,
  value: any,
  fromDefault: boolean
): void {
  if (
    field === 'parents' ||
    field === 'children' ||
    field === 'externalId' ||
    field === 'auth.role.id'
  ) {
    setInternalArrayStructure(id, field, value)
    return
  }

  if (isArray(value)) {
    value = cjson.encode(value)
  }

  if (type(value) === 'table') {
    if (!fromDefault && value.$merge === false) {
      const keys = redis.hkeys(id)

      removeFields(id, field, value, keys)
    }

    for (let key in value) {
      if (key[0] !== '$') {
        const item = value[key]
        const nestedField = field ? field + '.' + key : key
        if (type(item) === 'table') {
          setObject(id, nestedField, item)
        } else {
          setField(id, nestedField, item, false)
        }
      }
    }

    return
  }

  // should never happen but needed to make TS happy
  if (!field) {
    return
  }

  if (fromDefault) {
    redis.hsetnx(id, field, tostring(value))
  } else {
    redis.hset(id, field, tostring(value))
  }
}

function remove(payload: DeleteOptions): boolean {
  if (isString(payload)) {
    return deleteItem(payload)
  }

  return deleteItem(payload.$id, payload.$hierarchy)
}

function update(payload: SetOptions): Id | null {
  if (!payload.$id) {
    if (!payload.type) {
      return null
    }
    const itemType =
      type(payload.type) === 'string'
        ? payload.type
        : (<any>payload.type).$value
    if (
      (payload.externalId && type(payload.externalId) === 'string') ||
      isArray(<any>payload.externalId)
    ) {
      payload.$id = genId({
        type: itemType,
        externalId: <any>payload.externalId
      })
    } else {
      payload.$id = genId({ type: itemType })
    }
  }

  if (!payload.$id) {
    return null
  }

  const exists = redis.hexists(payload.$id, 'type')

  if (!exists) {
    if (!payload.type) {
      payload.type = getTypeFromId(payload.$id)
    }

    if (!payload.parents && payload.$id !== 'root') {
      payload.parents = { $add: 'root' }
    }
  }

  setField(payload.$id, null, payload, false)
  return payload.$id
}

// We always set an $id property before passing to redis
// returns Id for updates, boolean for deletes

export default function modify(
  payload: ModifyOptions[]
): (ModifyResult | null)[] {
  const results: (Id | boolean | null)[] = []
  logger.info(`Running modify with batch of ${payload.length}`)
  for (let i = 0; i < payload.length; i++) {
    let operation = payload[i]
    if (operation.kind === 'update') {
      results[i] = update(operation.payload)
      // TODO: how do we want to handle errors here?
      // if (!results[i]) {
      //   return redis.Error(`Unable to update`)
      // }
    } else {
      results[i] = remove(operation.payload)
    }
  }

  return results
}
