import { Meta, QuerySubscription, Fork } from './types'
import * as logger from '../../logger'
import { isFork } from './util'
import { getPrefixFromType } from '../../typeIdMapping'
import { indexOf, isArray } from '../../util'

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
          if (isArray(item.$value)) {
            for (let j = 0; j < item.$value.length; j++) {
              addType(item.$value[j], sub.type)
            }
          } else {
            addType(item.$value, sub.type)
          }
        } else if (item.$field === 'ancestors') {
          // add to member
        } else if (item.$field === 'ids') {
          // dont even know what to do here :D
          // prob need to add the traverse options and not the ids
        } else {
          sub.fields[item.$field] = true
        }
      }
    }
  }
}

// also need field and id here
function parseSubscriptions(
  querySubs: QuerySubscription[],
  meta: Meta,
  ids: string[],
  traverse?: string | string[]
) {
  const sub: QuerySubscription = {
    member: [],
    fields: {},
    type: []
  }

  if (meta.ast) {
    parseFork(meta.ast, sub)
  }

  sub.member[sub.member.length] = {
    $field: 'ancestors',
    $value: ids
  }

  // may need to add more here

  querySubs[querySubs.length] = sub
}

export default parseSubscriptions
