import * as redis from './redis'
import { isArray } from './util'
import { Id } from '../../client/src/schema/index'
import { ExternalId } from '../../client/src/set/types'
import { getPrefixFromType } from './typeIdMapping'

function hash(str?: string): string {
  return redis.id(str)
}

export type IdOptions = {
  type: string
  externalId?: ExternalId | ExternalId[]
}

export function id({ type, externalId }: IdOptions): Id | undefined {
  const prefix = getPrefixFromType(type)

  if (!prefix) {
    return undefined
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
