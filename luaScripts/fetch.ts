import get from '../lua/src/get/index'
import { GetOptions } from '~selva/get/types'
import * as logger from '../lua/src/logger'

logger.info('GET WITH: ' + ARGV[0])
const opts: GetOptions = cjson.decode(ARGV[0])
let a = get(opts)
// @ts-ignore
return cjson.encode(a)
