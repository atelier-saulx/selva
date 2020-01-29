const get = getOptions => {
  console.log('GET', getOptions)
}

const parseFilter = (result, filter) => {
  // opts
}

const parseNested = (result, opts) => {
  for (const k in opts) {
    console.log('-->', k)
  }
}

const parseQuery = (getOptions, id = 'root', field?) => {
  const result = { segments: [], nestedGet: {} }

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
    parseNested(result, getOptions)
  }

  // field just means get it for this field - only relevant for $sort
}

export default parseQuery
