import * as logger from '../../logger'
import { GetOptions, Find } from '~selva/get/types'
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
    // single find
    return [{ isFork: true }, 'Find outside of a list not supported']
    // return parseFind(opts.$find, id)
  }
  return [{ isFork: true }, 'Not a valid query']
}

const parseQuery = (
  getOptions: GetOptions,
  id: string = 'root',
  traverse?: string
): [any, string | null] => {
  if (getOptions.$list && getOptions.$find) {
    return [null, 'If using $list put $find in list']
  }

  // get object
  const resultGet = {}

  let ids: any[] | undefined = []
  let resultFork: Fork | undefined
  if (getOptions.$list || getOptions.$find) {
    const [r, err] = parseNested(getOptions, id, traverse)
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
    if (err) {
      return [null, err]
    }
    const args = createSearchArgs(getOptions, query, resultFork)
    printAst(resultFork, args)

    const queryResult: string[] = redis.call('ft.search', 'default', ...args)
    ids = queryResult
  }

  const results: any[] = []

  if (ids) {
    const find = getFind(getOptions)
    let nestedFind: GetOptions | undefined
    if (find && find.$find) {
      // if nestedFind
      if (getOptions.$list) {
        nestedFind = {
          $list: {
            $find: find.$find
          }
        }
        // WILL NOT WORK YET - NEED TO SORT YOURSELF
        // if (getOptions.$list.$sort) {
        //   nestedFind.$list.$sort = getOptions.$list.$sort
        // }
        // if (getOptions.$list.$range) {
        //   nestedFind.$list.$range = getOptions.$list.$range
        // }
      } else {
        nestedFind = { $find: find.$find }
      }
    }

    for (let i = 1; i < ids.length; i++) {
      const opts: GetOptions = { $id: ids[i] }
      for (let key in getOptions) {
        if (key !== '$find' && key !== '$list' && key !== '$id') {
          opts[key] = getOptions[key]
        }
      }
      if (nestedFind) {
        for (let key in nestedFind) {
          opts[key] = nestedFind[key]
        }
        opts.id = true
        const arr = queryGet(opts)
        // if sort do this smarter
        logger.info(arr)

        for (let j = 0; j < arr.length; j++) {
          // need id to compare
          if (!getOptions.id) {
            delete arr[j].id
          }

          results[results.length] = arr[j]
        }
      } else {
        results[results.length] = get(opts)
      }
      // WILL NOT WORK YET - RANGE - also if not a query need to do range yourself
      // if (nestedFind) and range
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
