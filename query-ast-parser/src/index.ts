import printAst from './printAst'
import isFork from './isFork'
import ast2rpn from './ast2rpn'

import createAst from './parseFilters'

import convertNow from './convertNow'

import { Filter } from './types'

import { Rpn } from './types'

const createRpn = (filters: Filter | Filter[]): Rpn | void => {
  if (!Array.isArray(filters)) {
    filters = [filters]
  }
  const fork = createAst(filters)
  if (fork) {
    return ast2rpn(fork)
  }
}

export * from './types'

export { printAst, isFork, createAst, ast2rpn, createRpn, convertNow }
