import { isArray, joinAny, joinString } from '../../util'
import { FilterAST, Fork, Value } from './types'
import { isFork } from './util'
import * as logger from '../../logger'

const returnNumber = (filter, value: Value): string => {
  if (filter.$operator === '>') {
    return `(@${filter.$field}:[${tostring(value)},inf])`
  } else if (filter.$operator === '<') {
    return `(@${filter.$field}:[-inf,${tostring(value)}])`
  } else if (filter.$operator === '..') {
    return `(@${filter.$field}:[${tostring(value[0])},${tostring(value[1])}])`
  } else if (filter.$operator === '!=') {
    return `(-(@${filter.$field}:[${tostring(value)},${tostring(value)}]))`
  } else if (filter.$operator === '=') {
    return `(@${filter.$field}:[${tostring(value)},${tostring(value)}])`
  }
  return ''
}

const addField = (filter: FilterAST): string => {
  // depends on field type
  const type = filter.$search && filter.$search[0]
  const operator = filter.$operator
  if (type === 'TAG') {
    if (isArray(filter.$value)) {
      filter.$value = `${joinAny(filter.$value, '|')}`
    }
    if (operator === '!=') {
      return `(-(@${filter.$field}:{${filter.$value}}))`
    } else if (operator === '=') {
      return `(@${filter.$field}:{${filter.$value}})`
    }
  } else if (type === 'NUMERIC') {
    if (isArray(filter.$value) && filter.$operator !== '..') {
      let value = ''
      for (let i = 0, len = filter.$value.length; i < len; i++) {
        const v = returnNumber(filter, filter.$value[i])
        value += value !== '' ? '|' + v : v
      }
      return `(${value})`
    } else {
      return returnNumber(filter, filter.$value)
    }
  } else if (type === 'TEXT') {
    // equals will be a partial here
    // DO THINGS
    // INCLUDE LANGUAGE ETC
  } else if (type === 'GEO') {
    // later
  }
  return ''
}

function createSearchString(filters: Fork): [string, string | null] {
  const searchString: string[] = []
  if (filters.$and) {
    for (let filter of filters.$and) {
      if (!isFork(filter)) {
        if (filter.$field !== 'id') {
          searchString[searchString.length] = addField(filter)
        }
      } else {
        const [nestedSearch, err] = createSearchString(filter)
        if (err) {
          return ['', err]
        }
        searchString[searchString.length] = nestedSearch
      }
    }
    return [`(${joinString(searchString, ' ')})`, null]
  } else if (filters.$or) {
    for (let filter of filters.$or) {
      if (isFork(filter) && filter.$or) {
        const [nestedSearch, err] = createSearchString(filter)
        if (err) {
          return ['', err]
        }
        searchString[searchString.length] = nestedSearch
      } else if (!isFork(filter)) {
        if (filter.$field !== 'id') {
          searchString[searchString.length] = addField(filter)
        }
      } else {
        const [nestedSearch, err] = createSearchString(filter)
        if (err) {
          return ['', err]
        }
        searchString[searchString.length] = nestedSearch
      }
    }
    return [`(${joinString(searchString, '|')})`, null]
  }
  return ['', 'No valid cases for createSearchString']
}

export default createSearchString
