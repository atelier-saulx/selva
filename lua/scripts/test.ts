import { add } from '../src/testmodule'

const result = add(tonumber(ARGV[0]), tonumber(ARGV[1]))

// @ts-ignore
return result
