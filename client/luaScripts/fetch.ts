import get from '../lua/src/get/index'
import { GetOptions } from '~selva/get/types'
import * as logger from '../lua/src/logger'
import { splitString } from '../lua/src/util'
import globals from '../lua/src/globals'

globals.NEEDS_GSUB = false

let [loglevel, clientId = null] = splitString(ARGV[0], ':')
logger.configureLogger(clientId, <logger.LogLevel>loglevel)

const opts: GetOptions = cjson.decode(ARGV[1])
let a = get(opts)

let encoded: string = cjson.encode(a)
if (globals.NEEDS_GSUB) {
  ;[encoded] = string.gsub(encoded, '"___selva_empty_array"', '')
}

// @ts-ignore
return encoded
