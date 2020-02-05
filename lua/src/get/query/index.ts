import * as logger from '../../logger'
import { GetOptions } from '~selva/get/types'
import { Schema } from '~selva/schema/index'
// import createSearchString from './createSearchString'
import { getSchema } from '../../schema/index'
import parseFind from './parseFind'
// import createSearchArgs from './createSearchArgs'
import { QeuryResult } from './types'

const parseNested = (
  result: QeuryResult,
  opts: GetOptions,
  id: string,
  schema: Schema,
  field?: string
): string | null => {
  if (opts.$list) {
    if (opts.$list.$find) {
      const err = parseFind(result, opts.$list.$find, id, schema, field)
      if (err) {
        return err
      }
    } else if (opts.$sort) {
      // not yet!
      return 'Sort not implemented yet!'
    }
  } else if (opts.$find) {
    const err = parseFind(result, opts.$find, id, schema, field)
    if (err) {
      return err
    }
  } else {
    return 'Should not come here no valid query'
  }
  return null
}

const parseQuery = (
  getOptions: GetOptions,
  schema: Schema,
  id: string = 'root',
  field?: string
): [any, string | null] => {
  // top level result
  const result: QeuryResult = {
    filters: { $and: [], isFork: true },
    reverseMap: {}
  }

  const resultGet = {}

  if (getOptions.$list && !getOptions.$list.$find && !getOptions.$list.$sort) {
    return [null, 'Not implemented $list without $find']
  }

  if (getOptions.$list && !getOptions.$list.$find && getOptions.$list.$sort) {
    if (!field && !id) {
      return [null, 'Need field and id for a filtered list + $sort']
    }
    // field can be nested (using . notation)
    // will only work for indexed fields - read schema!
  }

  if (getOptions.$list && getOptions.$find) {
    return [null, 'If using $list put $find in list']
  }

  if (getOptions.$list || getOptions.$find) {
    const err = parseNested(result, getOptions, id, schema, field)
    if (err) {
      return [null, err]
    }
    for (let key in getOptions) {
      if (key !== '$list' && key !== '$find') {
        resultGet[key] = getOptions[key]
      }
    }
  }

  logger.info('got result!', result.filters)

  // const [qeury, err] = createSearchString(result.filters, schema)

  // if (err) {
  //   return [null, err]
  // }

  // logger.info('got search string', qeury)

  // const args = createSearchArgs(getOptions, qeury)

  // logger.info(args)
  // const queryResult = redis.call('ft.search', 'default', ...args)

  // ftSearch(
  //   'default',
  //   ...createSearchArgs(getOptions, qeury)
  // )

  // logger.info(queryResult[0])
  // const r = await Promise.all(
  //   queryResult.slice(1).map((id: string) => {
  //     const opts = Object.assign({}, getOptions, { $id: id })
  //     return client.get(opts)
  //   })
  // )

  return [[], null]

  // return [queryResult, null]
}

const queryGet = (getOptions: GetOptions): any => {
  const id = getOptions.$id || 'root'
  const schema = getSchema()
  const [result, err] = parseQuery(getOptions, schema, id)
  if (err) {
    logger.error(err)
  }
  return result
}

export default queryGet
