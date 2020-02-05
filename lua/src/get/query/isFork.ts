import { Fork, FilterAST } from './types'

export default function isFork(x: any): x is Fork {
  return type(x) === 'table' && x.isFork
}
