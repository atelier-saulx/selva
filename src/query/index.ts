import compareFilters from './compareFilters'
import createSearchString from './createSearchString'
import parseFilters from './parseFilters'
import addResult from './addResult'

// need this from lua
const parseType = type => {
  if (!Array.isArray(type)) {
    type = [type]
  }
  return type.map(v => {
    if (typeof v === 'string') {
      return getPrefix(v) + '*'
    } else if (Array.isArray(v)) {
      return v.map(v => getPrefix(v) + '*')
    }
  })
}

const getPrefix = type => {
  return type.slice(0, 2)
}

const exampleSchema = {}

// parse get options
const get = getOptions => {
  console.log('GET', getOptions)
}

const parseFind = (result, opts, id, field, schema) => {
  const { $traverse, $filter, $find } = opts
  if ($traverse) {
    if ($traverse === 'descendants') {
      if ($filter) {
        const ancestorFilter = compareFilters(
          result,
          {
            $field: 'ancestors',
            $value: id,
            $operator: '='
          },
          schema
        )
        if (ancestorFilter) {
          addResult(result, ancestorFilter, '$and')
        }
        parseFilters(result, $filter, schema)
      }
    } else if ($traverse === 'ancestors') {
      if ($filter) {
        // if id or type can do something smart - else nested query on the results
      } else {
        // just return the ancestors
      }
    } else if ($traverse === 'children') {
    } else if ($traverse === 'parents') {
    }

    if ($find) {
      parseFind(result, $find, id, field, schema)
    }
  } else {
    throw new Error('Need to allways define $traverse for now')
  }
}

const parseNested = (result, opts, id, field, schema) => {
  if (opts.$list) {
    if (opts.$list.$find) {
      parseFind(result, opts.$list.$find, id, field, schema)
    } else if (opts.$sort) {
      console.log('sort not implemented yet')
      // not yet!
    }
  } else if (opts.$find) {
    parseFind(result, opts.$find, id, field, schema)
  } else {
    throw new Error('should not come here no valid query')
    // sort perhaps?
  }
}

const parseQuery = (getOptions, id = 'root', field?) => {
  const result = { filters: {}, reverseMap: {} }

  const schema = exampleSchema

  // field needs to be included together with id
  if (getOptions.$list && !getOptions.$list.$find && !getOptions.$list.$sort) {
    console.log('just normal list no query needed')
    return false
  }

  if (getOptions.$list && !getOptions.$list.$find && getOptions.$list.$sort) {
    if (!field && !id) {
      throw new Error('need field and id for a filtered list + $sort')
    }
    // field can be nested (using . notation)
    // will only work for indexed fields - read schema!
  }

  if (getOptions.$list && getOptions.$find) {
    throw new Error('if using $list put $find in list')
  }

  if (getOptions.$list || getOptions.$find) {
    parseNested(result, getOptions, id, field, schema)
  }

  console.dir(result.filters, { depth: 10 })

  const r = createSearchString(result.filters, schema).slice(1, -1)

  console.log('\n\n')
  console.log(r)

  // field just means get it for this field - only relevant for $sort
}

export default parseQuery
