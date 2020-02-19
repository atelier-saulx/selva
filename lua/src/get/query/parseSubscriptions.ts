import { Meta, QuerySubscription, Fork } from './types'
import * as logger from '../../logger'
import { isFork } from './util'
import { getPrefixFromType } from '../../typeIdMapping'
import { indexOf, isArray, joinString } from '../../util'
import { GetOptions } from '~selva/get/types'

const addType = (type: string | number, arr: string[]) => {
  const prefix = getPrefixFromType(tostring(type))
  if (indexOf(arr, prefix) === -1) {
    arr[arr.length] = prefix
  }
}

function parseFork(ast: Fork, sub: QuerySubscription) {
  const list = ast.$and || ast.$or
  if (list) {
    for (let i = 0; i < list.length; i++) {
      const item = list[i]
      if (isFork(item)) {
        parseFork(item, sub)
      } else {
        if (item.$field === 'type') {
          if (!sub.type) {
            sub.type = []
          }
          // FIXME: Not completely correct unfortunately
          // for now it just checks if one of the types matches (bit slower but fine)
          if (isArray(item.$value)) {
            for (let j = 0; j < item.$value.length; j++) {
              addType(item.$value[j], sub.type)
            }
          } else {
            addType(item.$value, sub.type)
          }
        } else if (item.$field === 'ancestors') {
          const ancestors: { $field: string; $value: string[] } = {
            $field: 'ancestors',
            $value: []
          }

          const value = !isArray(item.$value) ? [item.$value] : item.$value

          for (let j = 0; j < value.length; j++) {
            ancestors.$value[ancestors.$value.length] = tostring(value[j])
          }

          sub.member[sub.member.length] = ancestors
          // add to member
        } else if (item.$field === 'id') {
          if (!sub.ids) {
            sub.ids = {}
          }
          const value = !isArray(item.$value) ? [item.$value] : item.$value
          for (let j = 0; j < value.length; j++) {
            sub.ids[value[j]] = true
          }
          // dont even know what to do here :D
          // prob need to add the traverse options and not the ids
        } else {
          sub.fields[item.$field] = true
        }
      }
    }
  }
}

const parseGet = (
  opts: GetOptions,
  fields: Record<string, true>,
  field: string[]
) => {
  for (let key in opts) {
    if (key[0] !== '$') {
      const item = opts[key]
      if (typeof item === 'object') {
        if (!item.$list) {
          const newArray: string[] = []
          for (let i = 0; i < field.length; i++) {
            newArray[newArray.length] = field[i]
          }
          newArray[newArray.length] = key
          parseGet(item, fields, newArray)
        }
      } else {
        fields[
          field.length > 0 ? joinString(field, '.') + '.' + key : key
        ] = true
      }
    }
  }
}

function parseSubscriptions(
  querySubs: QuerySubscription[],
  meta: Meta,
  ids: string[],
  getOptions: GetOptions,
  traverse?: string | string[]
) {
  let sub: QuerySubscription | undefined

  const queryId = redis.sha1hex(cjson.encode(getOptions))

  for (let i = 0; i < querySubs.length; i++) {
    if (querySubs[i].queryId === queryId) {
      sub = querySubs[i]
      break
    }
  }
  if (!sub) {
    sub = {
      member: [],
      fields: {},
      queryId
    }
    querySubs[querySubs.length] = sub
  }

  parseGet(getOptions, sub.fields, [])

  if (meta.ast) {
    parseFork(meta.ast, sub)
  }

  if (sub.ids || !meta.ast) {
    if (!sub.idFields) {
      sub.idFields = {}
    }
    const field = meta.traverse || traverse
    if (!field) {
      logger.error('WRONG MISSING FIELD or TRAVERSE')
    } else {
      for (let i = 0; i < ids.length; i++) {
        sub.idFields[`${ids[i]}.${field}`] = true
      }
    }
  }

  let sort = meta.sort
  if (sort) {
    if (!isArray(sort)) {
      sort = [sort]
    }
    for (let i = 0; i < sort.length; i++) {
      sub.fields[sort[i].$field] = true
    }
  }
}

export default parseSubscriptions
