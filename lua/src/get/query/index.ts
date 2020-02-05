import * as logger from '../../logger'
import { GetOptions } from '~selva/get/types'
import createSearchString from './createSearchString'
import parseFind from './parseFind/index'
import createSearchArgs from './createSearchArgs'
import { Fork } from './types'
import printAst from './printAst'
import get from '../index'
import isFork from './isFork'

const parseNested = (
  opts: GetOptions,
  id: string,
  field?: string
): [Fork | string[], string | null] => {
  // field is different will create its own find
  if (opts.$list) {
    if (opts.$list.$find) {
      return parseFind(opts.$list.$find, id)
    } else if (opts.$list.$sort) {
      return [{ isFork: true }, 'Sort without find not implemented yet!']
    }
    // same for range
  } else if (opts.$find) {
    return parseFind(opts.$find, id)
  }
  return [{ isFork: true }, 'Not a valid query']
}

const parseQuery = (
  getOptions: GetOptions,
  id: string = 'root',
  field?: string
): [any, string | null] => {
  // top level result
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

  let ids: any[] | undefined = []
  let resultFork: Fork | undefined
  if (getOptions.$list || getOptions.$find) {
    const [r, err] = parseNested(getOptions, id, field)
    if (err) {
      return [null, err]
    }
    for (let key in getOptions) {
      if (key !== '$list' && key !== '$find') {
        resultGet[key] = getOptions[key]
      }
    }
    if (isFork(r)) {
      resultFork = r
    } else {
      ids = r
    }
  }

  if (resultFork) {
    const [q, err] = createSearchString(resultFork)
    const query: string = q.substring(1, q.length - 1)
    printAst(resultFork, query)
    if (err) {
      return [null, err]
    }
    const args = createSearchArgs(getOptions, query, resultFork)
    const queryResult: string[] = redis.call('ft.search', 'default', ...args)
    ids = queryResult
  }

  const results: any[] = []

  if (ids) {
    for (let i = 1; i < ids.length; i++) {
      const opts: GetOptions = { $id: ids[i] }
      for (let key in getOptions) {
        if (key !== '$find' && key !== '$list' && key !== '$id') {
          opts[key] = getOptions[key]
        }
      }
      results[results.length] = get(opts)
    }
  }

  return [results, null]
}

const queryGet = (getOptions: GetOptions, id?: string): any[] => {
  if (!id) {
    id = getOptions.$id || 'root'
  }
  const [result, err] = parseQuery(getOptions, id)
  if (err) {
    logger.error(err)
  }
  return result
}

export default queryGet
