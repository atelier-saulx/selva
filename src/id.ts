// type list is important may need to be stored in the db (other types)
import { Type, ExternalId, Id, itemTypes } from './types'
const uuid = require('uuid')

console.log('???', uuid)
// import { SelvaClient } from './'

const hash = (str: string): string => {
  let hash = 5381
  let i = str.length
  while (i) hash = (hash * 33) ^ str.charCodeAt(--i)
  return (hash >>> 0).toString(16)
}

export const typePrefix = {}
export const inverseTypePrefix = {}

const createPrefix = (
  type: string,
  index: number,
  capital: boolean
): string => {
  // must be better!
  if (index > type.length) {
    return createPrefix(type, 0, true)
  }
  let prefix = type.slice(index, index + 2)
  if (capital === true) {
    prefix = prefix.toUpperCase()
  }
  if (typePrefix[prefix]) {
    return createPrefix(type, ++index, false)
  }
  inverseTypePrefix[type] = prefix
  typePrefix[prefix] = type
  return prefix
}

itemTypes.forEach((type: string) => createPrefix(type, 0, false))

type FnId = {
  type: Type
  externalId?: ExternalId | ExternalId[]
}

// client: SelvaClient,
function id({ type, externalId }: FnId): Id {
  // on connect need to use type map

  // need to handle new types
  // if type not in default type need to create something that does not collide
  // load types from db
  // needs to store in db will have colision now bad!!!

  // if not in default types inverseTypePrefix[type] then do something

  const prefix = inverseTypePrefix[type] || createPrefix(type, 0, false)
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
