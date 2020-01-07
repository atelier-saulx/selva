// type list is important may need to be stored in the db (other types)
import { Type, ExternalId, Id } from './types'

type FnId = {
  type: Type
  externalId?: ExternalId | ExternalId[]
}

function id({ type, externalId }: FnId): Id {
  console.info('hello', type, externalId)
  return 'poop'
}

export default id
