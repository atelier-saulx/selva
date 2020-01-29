import { Schema } from '../src/schema/index'
import { getSchema, updateSchema } from '../lua/src/schema/index'

const newSchema: Schema = cjson.decode(ARGV[0])

const [updated, err] = updateSchema(newSchema)
if (err && type(err) === 'string') {
  // @ts-ignore
  return redis.error_reply(err)
}

// @ts-ignore
return updated
