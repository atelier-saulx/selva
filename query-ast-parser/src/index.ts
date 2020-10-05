import printAst from './printAst'
import isFork from './isFork'
import ast2rpn from './ast2rpn'

import createAst from './parseFilters'

import { Filter } from '../../client/src/get/types'

const createRpn = (
  filters: Filter[]
): [string[] | undefined, string[]] | void => {
  const fork = createAst(filters)
  if (fork) {
    return ast2rpn(fork)
  }
}

export { printAst, isFork, createAst, ast2rpn, createRpn }
