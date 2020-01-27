import { Schema } from '../src/schema/index'
import { getSchema, saveSchema } from '../lua/src/schema/index'

const newSchema: Schema = cjson.decode(ARGV[0])
const oldSchema: Schema = getSchema()

if (!oldSchema || type(oldSchema) !== 'table') {
  // @ts-ignore
  return saveSchema(newSchema)
}

// @ts-ignore
return saveSchema('') // TODO
