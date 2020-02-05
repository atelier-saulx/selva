import { now, isArray } from '../../util'
import { Filter } from '~selva/get/types'
import { getSearchIndexes } from '../../schema/index'

function addSearch(filter: Filter): [string[], null | string] {
  const searchIndexes = getSearchIndexes()
  const search = searchIndexes.default && searchIndexes.default[filter.$field]
  if (search) {
    const operator = filter.$operator
    if (
      search[0] !== 'NUMERIC' &&
      (operator === '>' ||
        operator === '<' ||
        operator === '<=' ||
        operator === '..' ||
        operator === '>=')
    ) {
      return [
        [],
        `Cannot have numeric comparisons on other search types then NUMERIC ${filter.$field}`
      ]
    }
  } else {
    return [[], `Cannot search fields that are not indexed ${filter.$field}`]
  }
  if (search[0] === 'NUMERIC') {
    if (isArray(filter.$value)) {
      for (let i = 0; i < filter.$value.length; i++) {
        if (filter.$value[i] === 'now') {
          filter.$value[i] = now()
        }
      }
    }
    if (filter.$value === 'now') {
      filter.$value = now()
    }
  }
  return [search, null]
}

export default addSearch
