import { id as genId } from '../id'
import * as redis from '../redis'
import { Id } from '~selva/schema/index'
import { SetOptions } from '~selva/set/types'
import { getTypeFromId } from '../typeIdMapping'
import {
  isString,
  isArray,
  splitString,
  joinString,
  ensureArray,
  stringStartsWith,
  stringEndsWith
} from '../util'
import { resetSet, addToSet, removeFromSet } from './setOperations'
import { ModifyOptions, ModifyResult } from '~selva/modifyTypes'
import { DeleteOptions } from '~selva/delete/types'
import { deleteItem } from './delete'
import { reCalculateAncestors } from './ancestors'
import * as logger from '../logger'
import { addFieldToSearch } from './search'
import sendEvent from './events'
import { setUpdatedAt, setCreatedAt } from './timestamps'
import { cleanUpSuggestions } from './delete'

function isSetPayload(value: any): boolean {
  if (isArray(value)) {
    return true
  } else if (type(value) === 'table') {
    if (value.$add || value.$delete || value.$value) {
      return true
    }
  }

  return false
}

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
  } else if (type(value) === 'string') {
    resetSet(id, field, ensureArray(value), update, hierarchy)
  } else if (value.$value) {
    resetSet(id, field, value, update, hierarchy)
  } else {
    if (value.$add) {
      addToSet(id, field, value.$add, update, hierarchy)
    }
    if (value.$delete) {
      removeFromSet(id, field, value.$delete, hierarchy)
    }
  }

  // if it's an object field, also set a set marker
  if (field.indexOf('.') !== -1) {
    redis.hset(id, field, '___selva_$set')
  }

  sendEvent(id, field, 'update')
}

function setObject(
  id: string,
  field: string,
  item: any,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
) {
  if (item.$value !== null) {
    setField(id, field, item, false, source)
  } else if (item.$default) {
    if (item.$increment) {
      const result = redis.hsetnx(id, field, item.$default)
      if (result === 0) {
        redis.hincrby(id, field, item.$increment)
        sendEvent(id, field, 'update')
      }
      return
    }

    setField(id, field, item.$default, true, source)
  } else if (item.$increment) {
    redis.hincrby(id, field, item.$increment)
    sendEvent(id, field, 'update')
  } else if (item.$ref) {
    redis.hset(id, `${field}.$ref`, item.$ref)
  } else {
    setField(id, field, item, false, source)
  }
}

function setField(
  id: string,
  field: string | null,
  value: any,
  fromDefault: boolean,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): void {
  if (isSetPayload(value) && field) {
    setInternalArrayStructure(id, field, value)
    return
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
          setObject(id, nestedField, item, source)
        } else {
          setField(id, nestedField, item, false, source)
        }
      }
    }

    return
  }

  // should never happen but needed to make TS happy
  if (!field) {
    return
  }

  if (source) {
    const sourceString: string | null = !source
      ? null
      : type(source) === 'string'
      ? source
      : (<any>source).$name

    if (sourceString && !(<any>source).$overwrite) {
      const currentSource = redis.hget(id, '$source_' + field)
      if (currentSource && currentSource !== '' && currentSource !== source) {
        // ignore updates from different sources
        return
      }
    } else if (sourceString && isArray((<any>source).$overwrite)) {
      const currentSource = redis.hget(id, '$source_' + field)

      const sourceAry = <string[]>(<any>source).$overwrite
      let matching = false
      for (const sourceId of sourceAry) {
        if (sourceId === currentSource) {
          matching = true
        }
      }

      if (!matching) {
        // ignore updates from different sources if no overwrite specified for this source
        return
      }
    }

    redis.hset(id, '$source_' + field, sourceString)
  }

  if (fromDefault) {
    redis.hsetnx(id, field, tostring(value))
  } else {
    redis.hset(id, field, tostring(value))
  }

  addFieldToSearch(id, field, value)
  sendEvent(id, field, 'update')
}

function remove(payload: DeleteOptions): boolean {
  if (isString(payload)) {
    return deleteItem(payload)
  }

  let keys = false
  for (const key in payload) {
    if (key[0] !== '$') {
      keys = true
    }
  }

  if (!keys) {
    return deleteItem(payload.$id, payload.$hierarchy)
  }

  removeSpecified(payload.$id, '', payload)
  return true
}

function removeSpecified(
  id: Id,
  path: string,
  payload: Record<string, any>
): string[] {
  let falses: string[] = []
  let onlyFalse = true
  for (const key in payload) {
    if (key[0] !== '$') {
      const keyPath = path === '' ? key : path + '.' + key

      if (payload[key] === true) {
        onlyFalse = false
        const val = redis.hget(id, keyPath)

        if (
          val === '___selva_$set' ||
          key === 'parents' ||
          key === 'children'
        ) {
          redis.del(id + '.' + keyPath)
        } else {
          cleanUpSuggestions(id, keyPath)
        }

        redis.hdel(id, keyPath)
      } else if (payload[key] === false) {
        falses[falses.length] = keyPath
      } else if (type(payload[key]) === 'table') {
        const nested = removeSpecified(id, keyPath, payload[key])
        for (const item of nested) {
          falses[falses.length] = item
        }
      }
    }
  }

  // only run this top level
  if (onlyFalse && path === '') {
    const allKeys = redis.hkeys(id)
    logger.info('allKeys', allKeys)
    logger.info('falses', falses)
    for (const key of allKeys) {
      let skip = false
      for (const setAsFalse of falses) {
        if (
          stringStartsWith(key, setAsFalse) ||
          stringEndsWith(key, '.ancestors') ||
          key === 'type' ||
          key === 'createdAt' ||
          key === 'updatedAt'
        ) {
          skip = true
          break
        }
      }

      if (!skip) {
        redis.hdel(id, key)
      }
    }
  }

  return falses
}

function update(payload: SetOptions): Id | null {
  if (!payload.$id) {
    if (payload.$alias) {
      const accessAliases: string[] = ensureArray(payload.$alias)
      for (const alias of accessAliases) {
        const id: string = redis.hget('___selva_aliases', alias)
        if (id && id !== '') {
          payload.$id = id
          break
        }
      }

      if (!payload.$id) {
        delete payload.$alias
        if (!payload.aliases) {
          payload.aliases = accessAliases
        }

        update(payload)
      }
    } else if (!payload.type) {
      return null
    } else {
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
  }

  if (!payload.$id) {
    return null
  }

  const exists = redis.hexists(payload.$id, 'type')

  if (!exists) {
    // we always end up here if it's a new record
    sendEvent(payload.$id, '', 'created')

    if (!payload.type) {
      payload.type = getTypeFromId(payload.$id)
    }

    if (!payload.parents && payload.$id !== 'root') {
      payload.parents = { $add: ['root'] }
    }

    setCreatedAt(payload, payload.$id, payload.type)
  } else {
    setUpdatedAt(payload, payload.$id, payload.type)
  }

  setField(payload.$id, null, payload, false, payload.$source)
  return payload.$id
}

// We always set an $id property before passing to redis
// returns Id for updates, boolean for deletes

export default function modify(
  payload: ModifyOptions[]
): (ModifyResult | null)[] {
  const results: (Id | boolean | null)[] = []
  for (let i = 0; i < payload.length; i++) {
    let operation = payload[i]
    // logger.info(`OPERATION ${operation.kind}`)
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

  reCalculateAncestors()

  return results
}
