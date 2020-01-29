import { Schema } from '../src/schema/index'
import { updateSchema } from '../lua/src/schema/index'

const newSchema: Schema = cjson.decode(ARGV[0])

// TODO: check and panic if SHA doesn't match what is currently in database
// and check for this error in client to re-fetch schema
const [updated, err] = updateSchema(newSchema)
if (err && type(err) === 'string') {
  // @ts-ignore
  return redis.error_reply(err)
}

// @ts-ignore
return updated
