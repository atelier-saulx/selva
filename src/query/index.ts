const get = getOptions => {
  console.log('GET', getOptions)
}

const parseFilterDescendants = filter => {
  if (!Array.isArray(filter)) {
    filter = [filter]
  }
  const query = []

  for (let i = 0, len = filter.length; i < len; i++) {
    const orSegment = filter[i]

    console.log(orSegment)

    query.push()
  }
}

const parseFind = (result, opts, id, field) => {
  const { $traverse, $filter, $find } = opts

  if ($traverse) {
    if ($traverse === 'descendants') {
      result.query.push({ field: 'ancestors', value: id })
      if ($filter) {
        console.log(parseFilterDescendants($filter))
      }
    } else if ($traverse === 'ancestors') {
      // different means you need to find the ancestors
    } else if ($traverse === 'children') {
    } else if ($traverse === 'parents') {
    }

    // from is important if from descendants , ancestors is easy
  } else {
    // if not from $find (with a field)
    console.log('need to define $traverse for now')
  }
}

const parseNested = (result, opts, id, field) => {
  if (opts.$list) {
    if (opts.$list.$find) {
      parseFind(result, opts.$list.$find, id, field)
    } else if (opts.$sort) {
      // not yet!
    }
  } else if (opts.$find) {
    parseFind(result, opts.$find, id, field)
  } else {
    // sort perhaps?
  }
}

// (@x:foo)|(@y:bar) or
// https://oss.redislabs.com/redisearch/Query_Syntax.html

// FT.SEARCH cars "@country:korea @engine:(diesel|hybrid) @class:suv"
// FT.EXPLAIN {index} {query}

const parseQuery = (getOptions, id = 'root', field?) => {
  const result = { query: [], nestedGet: {} }

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
    parseNested(result, getOptions, id, field)
  }

  // field just means get it for this field - only relevant for $sort
}

export default parseQuery
