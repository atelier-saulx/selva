// type list is important may need to be stored in the db (other types)
import { Type, ExternalId, Id, itemTypes } from './types'
const uuid = require('uuid')

// import { SelvaClient } from './'

const hash = (str: string): string => {
  let hash = 5381
  let i = str.length
  while (i) hash = (hash * 33) ^ str.charCodeAt(--i)
  return (hash >>> 0).toString(16)
}

export const typePrefix = {}
export const inverseTypePrefix = {}

// map needs to be added!
const createPrefix = (type: string, index: number): string => {
  if (index > type.length) {
    return createPrefix(type, 0)
  }
  let prefix = type.slice(index, index + 2)
  if (typePrefix[prefix]) {
    return createPrefix(type, ++index)
  }
  inverseTypePrefix[type] = prefix
  typePrefix[prefix] = type
  return prefix
}

itemTypes.forEach((type: string) => createPrefix(type, 0))

type FnId = {
  type: Type
  externalId?: ExternalId | ExternalId[]
}

// client: SelvaClient,
function id({ type, externalId }: FnId): Id {
  const prefix = inverseTypePrefix[type]

  if (!prefix) {
    // need to load non predefined types from redis
    throw Error(`TYPE NOT PREDEFINED ${type} WILL DO IT LATER`)
  }

  if (externalId) {
    return (
      prefix +
      hash(
        Array.isArray(externalId) ? externalId.join(',') : String(externalId)
      )
    )
  } else {
    return prefix + hash(uuid())
  }
}

export default id
