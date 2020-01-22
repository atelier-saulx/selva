// only types can be imported with `paths`, not supported by tstl
import { Id } from '../../src/schema/index'
import * as redis from './redis'

let typePrefix: Record<string, string>
let inverseTypePrefix: Record<string, string>

export function getTypeFromId(id: Id): string {
  if (id === 'root') {
    return 'root'
  }

  if (!typePrefix) {
    typePrefix = cjson.decode(redis.hget('schema', 'prefixes'))
  }

  return typePrefix[id.substring(0, 2)]
}

export function getPrefixFromType(prefix: string): string {
  if (!inverseTypePrefix) {
    inverseTypePrefix = cjson.decode(redis.hget('schema', 'types'))
  }

  return inverseTypePrefix[prefix]
}
