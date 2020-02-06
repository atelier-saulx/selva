import * as logger from '../../logger'
import { GetOptions, Find, GetResult } from '~selva/get/types'
import createSearchString from './createSearchString'
import parseFind from './parseFind/index'
import createSearchArgs from './createSearchArgs'
import { Fork } from './types'
import printAst from './printAst'
import get from '../index'
import { isFork, getFind } from './util'

// make a function hasQuery

// get find

const parseNested = (
  opts: GetOptions,
  id: string,
  traverse?: string
): [Fork | string[], string | null] => {
  if (opts.$list) {
    const needsQeury: boolean = !!opts.$list.$sort
    if (opts.$list.$find) {
      return parseFind(opts.$list.$find, id, needsQeury)
    } else {
      if (!traverse) {
        return [{ isFork: true }, '$list without find needs traverse']
      } else {
        const find = {
          $traverse: traverse
        }
        // if sort need a query
        return parseFind(find, id, needsQeury)
      }
    }
  } else if (opts.$find) {
    return [{ isFork: true }, 'Find outside of a list not supported']
  }
  return [{ isFork: true }, 'Not a valid query']
}

const parseQuery = (
  getOptions: GetOptions,
  id: string = 'root',
  traverse?: string
): [GetResult[], string | null] => {
  // get object
  const resultGet = {}
  const results: GetResult[] = []

  if (getOptions.$list && getOptions.$find) {
    return [results, 'If using $list put $find in list']
  }

  let ids: any[] | undefined = []
  let resultFork: Fork | undefined
  if (getOptions.$list || getOptions.$find) {
    const [r, err] = parseNested(getOptions, id, traverse)
    if (err) {
      return [results, err]
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
    if (err) {
      return [results, err]
    }
    const args = createSearchArgs(getOptions, query, resultFork)
    printAst(resultFork, args)

    const queryResult: string[] = redis.call('ft.search', 'default', ...args)
    ids = queryResult
  }

  if (ids) {
    const find = getFind(getOptions)
    let nestedFind: GetOptions | undefined
    let nestedMap: Record<string, boolean> | undefined
    if (find && find.$find) {
      // if nestedFind
      nestedMap = {}
      if (getOptions.$list) {
        nestedFind = {
          $list: {
            $find: find.$find
          }
        }

        // FIXME: nested sort and range
        if (getOptions.$list.$sort) {
          return [results, 'Nested find sort is not supported yet!']
        }
      }
    }

    for (let i = 1; i < ids.length; i++) {
      const opts: GetOptions = { $id: ids[i] }
      for (let key in getOptions) {
        if (key !== '$find' && key !== '$list' && key !== '$id') {
          opts[key] = getOptions[key]
        }
      }
      if (nestedFind && nestedMap) {
        for (let key in nestedFind) {
          opts[key] = nestedFind[key]
        }
        opts.id = true
        const [arr, err] = parseQuery(opts, ids[i])
        if (err) {
          return [results, err]
        }
        for (let j = 0; j < arr.length; j++) {
          const item = arr[j]
          // if (!nestedMap[item.id]) {
          // nestedMap[item.id] = true
          if (!getOptions.id && item.id) {
            delete item.id
          }
          results[results.length] = item
          // }
        }
      } else {
        results[results.length] = get(opts)
      }
    }
  }

  return [results, null]
}

const queryGet = (getOptions: GetOptions, id?: string): any[] => {
  // check if query
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
