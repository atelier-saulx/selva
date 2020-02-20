import { id, IdOptions } from '../lua/src/id'
import * as logger from '../lua/src/logger'

const idOpts: IdOptions = cjson.decode(ARGV[0])
let a = id(idOpts)
// @ts-ignore
return a
