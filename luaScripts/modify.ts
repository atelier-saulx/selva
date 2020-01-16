import modify from '../lua/src/modify/index'
import { ModifyOptions } from '~selva/modifyTypes'

const modifyArgs: ModifyOptions[] = cjson.decode(ARGV[0])
let a = modify(modifyArgs)
// @ts-ignore
return a
