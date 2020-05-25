import * as redis from './redis'
import { Id } from '../../src/schema/index'
import { getPrefixFromType } from './typeIdMapping'

function hash(str?: string): string {
  return redis.id(str)
}

export type IdOptions = {
  type: string
  db?: string
}

export function id({ type }: IdOptions): Id | undefined {
  const prefix = getPrefixFromType(type)

  if (!prefix) {
    return undefined
  }

  return prefix + hash()
}
