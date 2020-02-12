import { Schema } from '../client/src/schema/index'
import { updateSchema } from '../lua/src/schema/index'
import * as logger from '../lua/src/logger'
import { splitString } from '../lua/src/util'

let [loglevel, clientId = null] = splitString(ARGV[0], ':')
logger.configureLogger(clientId, <logger.LogLevel>loglevel)

const newSchema: Schema = cjson.decode(ARGV[1])

const [updated, err] = updateSchema(newSchema)
if (err && type(err) === 'string') {
  // @ts-ignore
  return redis.error_reply(err)
}

// @ts-ignore
return updated
