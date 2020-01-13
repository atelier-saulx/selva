import { add } from '../src/testmodule'

declare const ARGV: string[]
declare function tonumber(this: void, str: string): number

const result = add(tonumber(ARGV[0]), tonumber(ARGV[1]))
// @ts-ignore
return result
