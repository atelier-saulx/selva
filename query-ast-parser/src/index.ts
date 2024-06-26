import printAst from './printAst'
import isFork from './isFork'
import ast2rpn from './ast2rpn'
import bfsExpr2rpn from './bfsExpr2rpn'

import createAst from './parseFilters'

import optimizeTypeFilters from './optimizeTypeFilters'

import convertNow from './convertNow'

import { Filter } from './types'

import { Rpn } from './types'

const createRpn = (
  types: Record<string, { prefix?: string }>,
  filters: Filter | Filter[]
): Rpn | void => {
  if (!Array.isArray(filters)) {
    filters = [filters]
  }
  const fork = createAst(filters)
  if (fork) {
    return ast2rpn(types, fork)
  }
}

export * from './types'

export {
  printAst,
  isFork,
  createAst,
  ast2rpn,
  bfsExpr2rpn,
  createRpn,
  convertNow,
  optimizeTypeFilters,
}
