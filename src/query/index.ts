import compareFilters from './compareFilters'
import createSearchString from './createSearchString'
import parseFilters from './parseFilters'
import addResult from './addResult'
import { SelvaClient } from '../'
import { GetOptions } from '~selva/get'
import { GetSchemaResult } from '~selva/schema/getSchema'

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
      // easier
    } else if ($traverse === 'parents') {
      // easier
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

  const qeury = createSearchString(result.filters, schema)

  const $list = getOptions.$list
  let lo = 0
  let hi = 99999

  if ($list.$range) {
    lo = $list.$range[0]
    hi = $list.$range[1]
  }

  const sort = []

  if ($list.$sort) {
    sort.push('SORTBY')
    if (!Array.isArray($list.$sort)) {
      $list.$sort = [$list.$sort]
    }
    for (let i = 0; i < $list.$sort.length; i++) {
      let { $field, $order } = $list.$sort[i]
      if (!$order) {
        $order = 'asc'
      }
      sort.push($field, $order.toUpperCase())
    }
  }

  const searchArgs = [qeury, 'NOCONTENT', 'LIMIT', lo, hi, ...sort]

  console.log('SEARCH:', searchArgs)

  const queryResult = await client.redis.ftSearch('default', ...searchArgs)

  const r = await Promise.all(
    queryResult.slice(1).map((id: string) => {
      const opts = Object.assign({}, getOptions, { $id: id })
      return client.get(opts)
    })
  )

  return r
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
