import { Fork } from './types'
import { GetOptions, Find } from '~selva/get/types'

export function getFind(opts: GetOptions): Find | undefined {
  if (opts.$list && opts.$list.$find) {
    return opts.$list.$find
  } else if (opts.$find) {
    return opts.$find
  }
  return undefined
}

export function isFork(x: any): x is Fork {
  return type(x) === 'table' && x.isFork
}
