const get = getOptions => {
  console.log('GET', getOptions)
}

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

const parseFilterDescendants = (operators, filter) => {
  if (!Array.isArray(filter)) {
    filter = [filter]
  }

  // const operatorsList = ['=', '!=', '>', '<', '>=', '<=']
  // for validation

  for (let i = 0, len = filter.length; i < len; i++) {
    const segment = filter[i]
    let { $operator, $field, $value } = segment

    const isType = $field === 'type'

    if (isType) {
      $field = 'id'
    }

    if (!operators[$operator]) {
      operators[$operator] = { fields: {} }
    }

    const { fields } = operators[segment.$operator]

    if (!fields[$field]) {
      fields[$field] = []
    }

    let isEqual = false
    for (let i = 0; i < fields[$field].length; i++) {
      if (
        fields[$field][i].value === segment.value &&
        fields[$field].operator === segment.operator
      ) {
        isEqual = true
        break
      }
    }

    if (!isEqual) {
      if (isType) {
        $value = parseType($value)
      }

      if (Array.isArray($value)) {
        // storing a nested array is an AND
        fields[$field].push(...$value)
      } else {
        fields[$field].push($value)
      }
    }
  }

  //   console.dir(operators, { depth: 10 })
  // first equal operator
}

const getPrefix = type => {
  return type.slice(0, 2)
}

const parseFind = (result, opts, id, field) => {
  const { $traverse, $filter, $find } = opts

  if ($traverse) {
    if (!result.operators['=']) {
      result.operators['='] = { fields: {} }
    }
    if ($traverse === 'descendants') {
      // need info - is it a search tag

      // also other way arround ofc...

      if (!result.operators['='].fields.ancestors) {
        result.operators['='].fields.ancestors = []
        // and | or , array is or
      }
      if (!result.operators['='].fields.ancestors[0]) {
        result.operators['='].fields.ancestors.push([])
      }
      // its an AND
      result.operators['='].fields.ancestors[0].push(id)
      // '=': { fields: { ancestors: [ [ 'volleyball' ] ], type: [ 'match' ] } }
      // does not work for AND with different things
      // ignore and for now

      if ($filter) {
        parseFilterDescendants(result.operators, $filter)
      }
    } else if ($traverse === 'ancestors') {
      if (!result.operators['='].fields.ancestors) {
        result.operators['='].fields.ancestors = []
      }
      if (!result.operators['='].fields.ancestors[0]) {
        result.operators['='].fields.ancestors.push([])
      }

      if ($filter) {
        const operators = {}
        parseFilterDescendants(operators, $filter)
        // id or type can do something special else its a nested query

        if (operators['='] && operators['='].fields) {
          const fields = operators['='].fields
          if (fields.id) {
            result.operators['='].fields.ancestors[0].push(fields.id)
            delete fields.id
          }
          if (fields.type) {
            result.operators['='].fields.ancestors[0].push(
              parseType(fields.type)
            )
            delete fields.type
          }
        }

        let hasFields = false
        for (let o in operators) {
          if (hasFields) {
            break
          }
          for (let k in operators[o].fields) {
            hasFields = true
            console.log(
              'everything else then type and id in acnestors needs a 2 step query'
            )
            break
          }
        }
      }
    } else if ($traverse === 'children') {
    } else if ($traverse === 'parents') {
    }

    if ($find) {
      console.log('nested')
      parseFind(result, $find, id, field)
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
  const result = { operators: {}, nestedGet: {} }

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

  console.dir(result, { depth: 10 })

  // field just means get it for this field - only relevant for $sort
}

export default parseQuery
