// import compareFilters from './compareFilters'
// import addResult from './addResult'
import { Schema } from '~selva/schema/index'
import { QeuryResult, FilterAST, Fork } from './types'
import addSearch from './addSearch'
import * as logger from '../../logger'
import { ensureArray } from '../../util'
import { Filter } from '~selva/get/types'
import printAst from './printAst'

const convertFilter = (filterOpt: Filter): [Fork, string | null] => {
  const [search, err] = addSearch(filterOpt)

  const filter: FilterAST = {
    $value: filterOpt.$value,
    $operator: filterOpt.$operator,
    $field: filterOpt.$field,
    $search: search
  }

  if (err) {
    return [{ isFork: true }, err]
  }

  if (filterOpt.$or && filterOpt.$and) {
    const fork: Fork = { isFork: true, $or: [] }
    return [fork, null]
  } else if (filterOpt.$or) {
    const fork: Fork = { isFork: true, $or: [] }
    return [fork, null]
  } else if (filterOpt.$and) {
    const fork: Fork = { isFork: true, $and: [] }
    fork.$and = [filter]
    return [fork, null]
  } else {
    const fork: Fork = { isFork: true, $and: [] }
    fork.$and = [filter]
    return [fork, null]
  }
}

// only for top level
const convertFilters = ($filter: Filter[]): [Fork, string | null] => {
  const fork: Fork & { $and: (Fork | FilterAST)[] } = { isFork: true, $and: [] }

  for (let i = 0; i < $filter.length; i++) {
    const [nestedFork, err] = convertFilter($filter[i])
    if (err) {
      return [fork, err]
    }
    if (nestedFork.$and) {
      for (let j = 0; j < nestedFork.$and.length; j++) {
        // flatten it
        fork.$and[fork.$and.length] = nestedFork.$and[j]
      }
    } else if (nestedFork.$or) {
      fork.$and[fork.$and.length] = nestedFork
    }
  }

  printAst(fork)

  return [fork, null]
}

const parseFilters = (
  result: QeuryResult,
  $filter: Filter[],
  schema: Schema
): [QeuryResult, string | null] => {
  const [filters, err] = convertFilters($filter)
  if (err) {
    return [result, err]
  }
  return [result, null]
}

export default parseFilters
