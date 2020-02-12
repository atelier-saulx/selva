// only types can be imported with `paths`, not supported by tstl
import { Id, Schema } from '../../client/src/schema/index'
import { getSchema } from '../../lua/src/schema/index'
import * as logger from './logger'

let typePrefix: Record<string, string>
let inverseTypePrefix: Record<string, string>

export function getTypeFromId(id: Id): string {
  if (id === 'root') {
    return 'root'
  }

  if (!typePrefix) {
    const schema = getSchema()
    typePrefix = schema.prefixToTypeMapping || {}
  }

  return typePrefix[id.substring(0, 2)]
}

export function getPrefixFromType(prefix: string): string {
  if (!inverseTypePrefix) {
    inverseTypePrefix = {}
    const schema: Schema = getSchema()
    for (const typeName in schema.types) {
      const type = schema.types[typeName]
      if (type.prefix) {
        inverseTypePrefix[typeName] = type.prefix
      } else {
        logger.warn(`No prefix for type ${typeName}`)
      }
    }
  }

  return inverseTypePrefix[prefix]
}
