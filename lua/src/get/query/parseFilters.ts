import { FilterAST, Fork } from './types'
import addSearch from './addSearch'
import { Filter } from '~selva/get/types'
import { isFork } from './util'
import reduceAnd from './reduceAnd'

const addToOption = (
  prevList: (FilterAST | Fork)[],
  newList: (FilterAST | Fork)[]
) => {
  for (let i = 0; i < newList.length; i++) {
    const t = newList[i]
    if (isFork(t) && t.$and && t.$and.length === 1) {
      prevList[prevList.length] = t.$and[0]
    } else {
      prevList[prevList.length] = t
    }
  }
}

const convertFilter = (filterOpt: Filter): [Fork, string | null] => {
  const [search, err] = addSearch(filterOpt)
  if (err) {
    return [{ isFork: true }, err]
  }

  // validate operator

  // need to remove equal then

  const o = filterOpt.$operator
  if (
    !(
      o === '=' ||
      o === '>' ||
      o === '<' ||
      o === '..' ||
      o === '!=' ||
      o === 'distance'
    )
  ) {
    return [{ isFork: true }, `Invalid filter operator ${o}`]
  }

  const filter: FilterAST = {
    $value: filterOpt.$value,
    $operator: o,
    $field: filterOpt.$field,
    $search: search
  }
  if (filterOpt.$or && filterOpt.$and) {
    const fork: WithRequired<Fork, '$or'> = { isFork: true, $or: [] }
    const [orFork, err] = convertFilter(filterOpt.$or)
    if (err) {
      return [{ isFork: true }, err]
    }
    const [andFork, err2] = convertFilter(filterOpt.$and)
    if (err2) {
      return [{ isFork: true }, err2]
    }
    if (andFork.$and) {
      andFork.$and[andFork.$and.length] = filter
      fork.$or[fork.$or.length] = andFork
    } else {
      fork.$or[fork.$or.length] = filter
    }
    if (orFork.$or) {
      addToOption(fork.$or, orFork.$or)
    }
    return [fork, null]
  } else if (filterOpt.$or) {
    const [nestedFork, err] = convertFilter(filterOpt.$or)
    if (err) {
      return [{ isFork: true }, err]
    }
    const fork: WithRequired<Fork, '$or'> = {
      isFork: true,
      $or: [filter]
    }
    if (nestedFork.$and) {
      fork.$or[fork.$or.length] = nestedFork
    } else if (nestedFork.$or) {
      for (let i = 0; i < nestedFork.$or.length; i++) {
        fork.$or[fork.$or.length] = nestedFork.$or[i]
      }
    }
    return [fork, null]
  } else if (filterOpt.$and) {
    const [nestedFork, err] = convertFilter(filterOpt.$and)
    if (err) {
      return [{ isFork: true }, err]
    }
    const fork: WithRequired<Fork, '$and'> = {
      isFork: true,
      $and: [filter]
    }
    if (nestedFork.$and) {
      addToOption(fork.$and, nestedFork.$and)
    }
    if (nestedFork.$or) {
      fork.$and[fork.$and.length] = nestedFork
    }
    const reduceErr = reduceAnd(fork)
    return [fork, reduceErr]
  } else {
    const fork: Fork = { isFork: true, $and: [filter] }
    return [fork, null]
  }
}

const parseFilters = ($filter: Filter[]): [Fork, string | null] => {
  const fork: WithRequired<Fork, '$and'> = { isFork: true, $and: [] }
  for (let i = 0; i < $filter.length; i++) {
    const [nestedFork, err] = convertFilter($filter[i])
    if (err) {
      return [fork, err]
    }
    if (nestedFork.$and) {
      for (let j = 0; j < nestedFork.$and.length; j++) {
        fork.$and[fork.$and.length] = nestedFork.$and[j]
      }
    } else if (nestedFork.$or) {
      fork.$and[fork.$and.length] = nestedFork
    }
  }
  const err = reduceAnd(fork)
  return [fork, err]
}

export default parseFilters
