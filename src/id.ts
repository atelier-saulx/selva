// type list is important may need to be stored in the db (other types)
import { Type, ExternalId, Id, itemTypes } from './types'
// import { SelvaClient } from './'

export const typePrefix = {}
export const inverseTypePrefix = {}

itemTypes.forEach((type: string) => {
  const prefix = type.slice(0, 2)
  inverseTypePrefix[type] = prefix
  typePrefix[prefix] = type
})

console.log(typePrefix)

type FnId = {
  type: Type
  externalId?: ExternalId | ExternalId[]
}

// client: SelvaClient,
function id({ type, externalId }: FnId): Id {
  // need to handle new types
  // if type not in default type need to create something that does not collide
  // load types from db
  console.info('hello', type, externalId)
  return 'poop'
}

export default id
