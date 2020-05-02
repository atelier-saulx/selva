import { isArray } from '../util'
import * as redis from '../redis'
import * as logger from '../logger'

export default function allowSet(
  id: string,
  field: string,
  source?: string | { $overwrite?: boolean | string[]; $name: string }
): boolean {
  if (!source) {
    return true
  }

  const sourceString: string | null = !source
    ? null
    : type(source) === 'string'
    ? source
    : (<any>source).$name

  if (sourceString && !(<any>source).$overwrite) {
    const currentSource = redis.hget(id, '$source_' + field)
    if (currentSource && currentSource !== '' && currentSource !== source) {
      // ignore updates from different sources
      return false
    }
  } else if (sourceString && isArray((<any>source).$overwrite)) {
    const currentSource = redis.hget(id, '$source_' + field)

    const sourceAry = <string[]>(<any>source).$overwrite
    let matching = false
    for (const sourceId of sourceAry) {
      if (sourceId === currentSource) {
        matching = true
      }
    }

    if (!matching) {
      // ignore updates from different sources if no overwrite specified for this source
      return false
    }
  }

  redis.hset(id, '$source_' + field, sourceString)

  return true
}
