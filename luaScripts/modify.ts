import modify from '../lua/src/modify/index'
import { ModifyOptions } from '~selva/modifyTypes'

const modifyArgs: ModifyOptions[] = []
for (let i = 0; i < ARGV.length; i++) {
  modifyArgs[i] = cjson.decode(ARGV[i])
}
let a = modify(modifyArgs)
// @ts-ignore
return a
