import { FilterAST, Fork, Value, WithRequired } from './types'
import { Filter, GeoFilter } from './types'
import isFork from './isFork'
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

function isGeoFilterValue(x: any): x is GeoFilter {
  return !!x && x.$operator === 'distance'
}

function convertGeoFilterValue(geoFilter: GeoFilter): (string | number)[] {
  const { $lon, $lat, $radius } = geoFilter.$value
  return [$lon, $lat, $radius, 'm']
}

const convertFilter = (filterOpt: Filter): [Fork, string | null] => {
  //   const [search, err] = addSearch(filterOpt)

  const o = filterOpt.$operator
  if (
    !(
      o === '=' ||
      o === '>' ||
      o === '<' ||
      o === '..' ||
      o === '!=' ||
      o === 'has' ||
      o === 'distance' ||
      o === 'exists' ||
      o === 'notExists' ||
      o === 'textSearch'
    )
  ) {
    return [{ isFork: true }, `Invalid filter operator ${o}`]
  }

  const filter: FilterAST = {
    $operator: o,
    $field: filterOpt.$field,
  }

  if (o !== 'notExists' && o !== 'exists') {
    filter.$value = isGeoFilterValue(filterOpt)
      ? convertGeoFilterValue(filterOpt)
      : <Value>filterOpt.$value
  }

  let hasNow = false
  if (Array.isArray(filter.$value)) {
    for (const val of filter.$value) {
      if (typeof val === 'string' && val.startsWith('now')) {
        hasNow = true
        break
      }
    }
  } else {
    if (typeof filter.$value === 'string' && filter.$value.startsWith('now')) {
      hasNow = true
    }
  }

  if (hasNow) {
    filter.hasNow = true
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
      $or: [filter],
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
      $and: [filter],
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

const parseFilters = ($filter: Filter | Filter[]): Fork | void => {
  if (!Array.isArray($filter)) {
    $filter = [$filter]
  }

  const fork: WithRequired<Fork, '$and'> = { isFork: true, $and: [] }
  for (let i = 0; i < $filter.length; i++) {
    const [nestedFork, err] = convertFilter($filter[i])
    if (err) {
      return
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
  if (err) {
    return
  }
  return fork
}

export default parseFilters
