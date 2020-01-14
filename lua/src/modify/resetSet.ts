import { Id } from '~selva/schema'
import * as redis from '../redis'

export default function resetSet(id: string, field: string, value: Id[]): void {
  const setKey = `${id}.${field}`
  redis.del(setKey)
  redis.sadd(setKey, ...value)
}
