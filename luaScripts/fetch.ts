import get from '../lua/src/get/index'
import { GetOptions } from '~selva/get/types'
import * as logger from '../lua/src/logger'
import globals from '../lua/src/globals'

globals.NEEDS_GSUB = false

logger.info('GET WITH: ' + ARGV[0])
const opts: GetOptions = cjson.decode(ARGV[0])
let a = get(opts)

let encoded: string = cjson.encode(a)
if (globals.NEEDS_GSUB) {
  ;[encoded] = string.gsub(encoded, '"___selva_empty_array"', '')
}

// @ts-ignore
return encoded
