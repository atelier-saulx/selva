import compareFilters from './compareFilters'
import createSearchString from './createSearchString'
import parseFilters from './parseFilters'
import addResult from './addResult'
import { SelvaClient } from '../'
import { GetOptions } from '~selva/get'
import { GetSchemaResult } from '~selva/schema/getSchema'

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

const parseFind = async (
  client: SelvaClient,
  result,
  opts,
  id,
  field,
  schema: GetSchemaResult
): Promise<void> => {
  let { $traverse, $filter, $find } = opts
  if (!$filter) {
    $filter = opts.$filter = []
  }
  if (!Array.isArray($filter)) {
    $filter = opts.$filter = [$filter]
  }
  if ($traverse) {
    if ($traverse === 'descendants') {
      if ($filter) {
        $filter.push({
          $field: 'ancestors',
          $value: id,
          $operator: '='
        })
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
      await parseFind(client, result, $find, id, field, schema)
    }
  } else {
    throw new Error('Need to allways define $traverse for now')
  }
}

const parseNested = async (
  client: SelvaClient,
  result,
  opts,
  id,
  field,
  schema: GetSchemaResult
): Promise<void> => {
  if (opts.$list) {
    if (opts.$list.$find) {
      await parseFind(client, result, opts.$list.$find, id, field, schema)
    } else if (opts.$sort) {
      console.log('sort not implemented yet')
      // not yet!
    }
  } else if (opts.$find) {
    await parseFind(client, result, opts.$find, id, field, schema)
  } else {
    throw new Error('should not come here no valid query')
    // sort perhaps?
  }
}

const parseQuery = async (
  client: SelvaClient,
  getOptions: GetOptions,
  schema: GetSchemaResult,
  id = 'root',
  field?
): Promise<any> => {
  const result = { filters: {}, reverseMap: {} }
  let resultGet = {}
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
    await parseNested(client, result, getOptions, id, field, schema)

    for (let key in getOptions) {
      if (key !== '$list' && key !== '$find') {
        resultGet[key] = getOptions[key]
      }
    }
  }

  // console.dir(result.filters, { depth: 10 })

  // const qeury = createSearchString(result.filters, schema).slice(1, -1)

  const qeury = createSearchString(result.filters, schema)

  const getParameters = resultGet
  console.log(getParameters, qeury)
  // test '@type:{match}'
  // LIMIT 0 99999

  // @ancestors:{"root"}
  let queryResult = await client.redis.ftSearch(
    'default',
    qeury,
    'LIMIT',
    0,
    99999,
    'NOCONTENT'
  )

  if (queryResult.length === 1 && queryResult[0] === 0) {
    // means is empty...
    queryResult = []
  }

  // console.log(await client.redis.ftInfo('default'))

  const r = await Promise.all(
    queryResult.slice(1).map((id: string) => {
      const opts = Object.assign({}, getOptions, { $id: id })
      return client.get(opts)
    })
  )

  return r

  // console.log('\n\n')
  // console.log(r)

  // field just means get it for this field - only relevant for $sort
}

const queryGet = async (
  client: SelvaClient,
  getOptions: GetOptions
): Promise<any> => {
  const id = getOptions.$id || 'root'
  const schema = await client.getSchema()

  // identify if its part of a query here

  return await parseQuery(client, getOptions, schema, id)
}

export default queryGet
