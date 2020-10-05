import { Filter } from '../../client/src/get/types'

function addSearch(filter: Filter): [string[], null | string] {
  if (filter.$field === 'id') {
    return [['TAG'], null]
  }

  // pass schema here

  console.log('ADD SEARCH STILL SHAKY DONT WANT SEARCH SCHEMA!')
  console.log('trying to find search...', filter)

  //   const searchIndexes =  //getSearchIndexes()
  const search = false //= searchIndexes.default && searchIndexes.default[filter.$field]
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
    // this will be ignored after
    // return [[], null]
    return [[], `Cannot search fields that are not indexed ${filter.$field}`]
  }

  return [search, null]
}

export default addSearch
