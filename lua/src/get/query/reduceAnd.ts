import { Fork, FilterAST, Value } from './types'
import printAst from './printAst'
import { isArray, indexOf } from '../../util'
import isFork from './isFork'
import * as logger from '../../logger'

const valueIsEqual = (a: Value, b: Value, strict: boolean): boolean => {
  if (a === b) {
    return true
  }
  if (isArray(a) && isArray(b)) {
    if (a.length !== b.length) {
      return false
    }
    for (let i = 0; i < a.length; i++) {
      if (strict ? b[i] !== a[i] : indexOf(b, a[i]) == -1) {
        return false
      }
    }
    return true
  }
  return false
}

const filterIsEqual = (a: FilterAST, b: FilterAST): boolean => {
  return (
    a.$operator === b.$operator &&
    a.$field === b.$field &&
    valueIsEqual(a.$value, b.$value, a.$operator === '..')
  )
}

function reduceAnd(fork: Fork & { $and: (Fork | FilterAST)[] }): string | null {
  printAst(fork, 'GO REDUCE THIS')
  const reduced: Record<string, FilterAST[]> = {}
  for (let i = 0; i < fork.$and.length; i++) {
    const filter = fork.$and[i]
    if (!isFork(filter)) {
      if (!reduced[filter.$field]) {
        reduced[filter.$field] = []
      }
      const filters = reduced[filter.$field]
      let addFilter = true
      for (let j = 0; j < filters.length; j++) {
        const prevFilter = filters[j]
        if (filterIsEqual(prevFilter, filter)) {
          addFilter = false
          break
        } else {
          // all comparisons!!!
          // make it nice
        }
      }
      if (addFilter) {
        filters[filters.length - 1] = filter
      }
    }
  }
  return null
}

export default reduceAnd
