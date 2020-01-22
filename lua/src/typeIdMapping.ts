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
    for (const key in typePrefix) {
      inverseTypePrefix[typePrefix[key]] = key
    }
  }

  return typePrefix[id.substring(0, 2)]
}

export function getPrefixFromType(prefix: string): string {
  if (!typePrefix) {
    typePrefix = cjson.decode(redis.hget('schema', 'prefixes'))
    for (const key in typePrefix) {
      inverseTypePrefix[typePrefix[key]] = key
    }
  }

  return inverseTypePrefix[prefix]
}
