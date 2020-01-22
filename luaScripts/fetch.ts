import get from '../lua/src/get/index'
import { GetOptions } from '~selva/get/types'

const opts: GetOptions = cjson.decode(ARGV[0])
let a = get(opts)
// @ts-ignore
return a
