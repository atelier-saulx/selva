import { now, isArray } from '../../util'
import { Filter } from '~selva/get/types'
import { getSearchIndexes } from '../../schema/index'
import * as logger from '../../logger'

function addSearch(filter: Filter): [string[], boolean, null | string] {
  if (filter.$field === 'id') {
    return [['TAG'], false, null]
  }

  let hasNow = false
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
        false,
        `Cannot have numeric comparisons on other search types then NUMERIC ${filter.$field}`
      ]
    }
  } else {
    return [
      [],
      false,
      `Cannot search fields that are not indexed ${filter.$field}`
    ]
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
      logger.info('NOWNOWNOW')
      hasNow = true
      filter.$value = now()
    }
  }
  return [search, hasNow, null]
}

export default addSearch
