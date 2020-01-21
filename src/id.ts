// type list is important may need to be stored in the db (other types)
// import { Type, ExternalId, Id, inverseTypePrefix } from './schema'
import uuid from 'uuid'
import { SelvaClient } from './'

const inverseTypePrefix = {}

const hash = (str: string): string => {
  let hash = 5381
  let i = str.length
  while (i) hash = (hash * 33) ^ str.charCodeAt(--i)
  return (hash >>> 0).toString(16)
}

type IdOptions = {
  type: string
  externalId?: string | string[]
}

function id(client: SelvaClient, { type, externalId }: IdOptions): string {
  const prefix = inverseTypePrefix[type]

  if (!prefix) {
    // need to load non predefined types from redis
    throw Error(`TYPE NOT PRE-DEFINED ${type} WILL DO IT LATER`)
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

export { id, IdOptions }
