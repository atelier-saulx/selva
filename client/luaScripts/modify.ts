import modify from '../lua/src/modify/index'
import { getSchema } from '../lua/src/schema/index'
import { ModifyOptions } from '~selva/modifyTypes'
import * as logger from '../lua/src/logger'
import { splitString } from '../lua/src/util'

const currentSchema = getSchema()

let [loglevel, clientId = null] = splitString(ARGV[0], ':')
logger.configureLogger(clientId, <logger.LogLevel>loglevel)

const modifyArgs: ModifyOptions[] = []
for (let i = 0, j = 0; i < ARGV.length; i += 3, j++) {
  const sha = ARGV[i + 1]
  // if (currentSchema.sha && sha !== currentSchema.sha) {
  //   // @ts-ignore
  //   return redis.error_reply(
  //     'SHA mismatch: trying to update an older schema version please re-fetch and try again'
  //   )
  // }
  modifyArgs[j] = cjson.decode(ARGV[i + 2])
}
let a = modify(modifyArgs)
// @ts-ignore
return a
