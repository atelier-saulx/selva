// type list is important may need to be stored in the db (other types)
import { Type, ExternalId, Id, inverseTypePrefix } from './schema'
import uuid from 'uuid'

const hash = (str: string): string => {
  let hash = 5381
  let i = str.length
  while (i) hash = (hash * 33) ^ str.charCodeAt(--i)
  return (hash >>> 0).toString(16)
}

function id({
  type,
  externalId
}: {
  type: Type
  externalId?: ExternalId | ExternalId[]
}): Id {
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

export default id
