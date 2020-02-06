import { Fork } from './types'

export function isFork(x: any): x is Fork {
  return type(x) === 'table' && x.isFork
}
