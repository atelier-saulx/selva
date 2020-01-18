import * as redis from './redis'
import { isArray } from './util'
import { Id, Type, ExternalId, inverseTypePrefix } from '~selva/schema'

function hash(str?: string): string {
  return redis.id(str)
}

type IdOptions = {
  type: Type
  externalId?: ExternalId | ExternalId[]
}

export function id({ type, externalId }: IdOptions): Id {
  const prefix = inverseTypePrefix[type]

  if (!prefix) {
    throw new Error(`Type not pre-defined ${type}`)
  }

  if (externalId) {
    let externalIdStr = ''
    if (isArray(externalId)) {
      for (let i = 0; i < externalId.length; i++) {
        externalIdStr += tostring(externalId[i])
      }
    }
    return prefix + hash(externalIdStr)
  }

  return prefix + hash()
}
