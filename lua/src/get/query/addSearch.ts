import { now, isArray } from '../../util'
import { Filter } from '~selva/get/types'
import { getSearchIndexes } from '../../schema/index'

function addSearch(filter: Filter): [string[], null | string] {
  if (filter.$field === 'id') {
    return [['TAG'], null]
  }

  const searchIndexes = getSearchIndexes()
  const search = searchIndexes.default && searchIndexes.default[filter.$field]
  if (search) {
    const operator = filter.$operator
    if (
      search[0] !== 'NUMERIC' &&
      (operator === '>' || operator === '<' || operator === '..')
    ) {
      return [
        [],
        `Cannot have numeric comparisons on other search types then NUMERIC ${filter.$field}`
      ]
    }
  } else {
    return [[], `Cannot search fields that are not indexed ${filter.$field}`]
  }

  return [search, null]
}

export default addSearch
