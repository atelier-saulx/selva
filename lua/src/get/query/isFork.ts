import { Fork, FilterAST } from './types'

export default function isFork(x: Fork | FilterAST): x is Fork {
  return (<any>x).isFork
}
