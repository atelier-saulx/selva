import * as redis from '../redis'
import { Id } from '~selva/schema'
import { SetOptions } from '~selva/setTypes'
import getTypeFromId from '../getTypeFromId'
import { isArray, ensureArray, splitString, joinString } from '../util'
import { resetSet, addToSet, removeFromSet } from './setOperations'

function removeFields(
  id: string,
  field: string | null,
  value: object,
  keys: string[]
): void {
  const path = field ? splitString('field', '.') : []
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
    resetSet(id, field, value, modify, hierarchy)
    return
  }

  if (value.$value) {
    resetSet(id, field, value, modify, hierarchy)
    return
  }

  if (value.$add) {
    value.$add = ensureArray(value.$add)
    addToSet(id, field, value.$add, modify, hierarchy)
  }
  if (value.$delete) {
    value.$delete = ensureArray(value.$delete)
    removeFromSet(id, field, value.$add, hierarchy)
  }
}

function setObject(id: string, field: string, item: any) {
  if (item.$value !== null) {
    setField(id, field, item, false)
  } else if (item.$default) {
    if (item.$increment) {
      const result = redis.hsetnx(id, field, item)
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
  if (!fromDefault && value.$merge === false) {
    const keys = redis.hkeys(id)
    removeFields(id, field, value, keys)
  }

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
    redis.hsetnx(id, field, value)
  } else {
    redis.hset(id, field, value)
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

  setField(payload.$id, null, payload, false)
  return payload.$id
}
