import query from '../lua/src/get/query/index'
import { GetOptions } from '~selva/get/types'
import * as logger from '../lua/src/logger'
import globals from '../lua/src/globals'
import get from '../lua/src/get/index'

const opts: GetOptions = cjson.decode(ARGV[0])
let a = query(get, {}, opts)

const encoded = cjson.encode(a)
// @ts-ignore
return encoded
