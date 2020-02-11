import * as logger from '../../logger'
import { GetOptions, GetResult } from '~selva/get/types'
import createSearchString from './createSearchString'
import parseFind from './parseFind/index'
import createSearchArgs from './createSearchArgs'
import { Fork } from './types'
import printAst from './printAst'
import { isFork, getFind } from './util'
import { emptyArray } from '../../util'
import { getSchema } from '../../schema/index'
import { GetFieldFn } from '../types'

const parseNested = (
  opts: GetOptions,
  ids: string[],
  traverse?: string
): [Fork | string[], string | null] => {
  if (opts.$list) {
    const needsQeury: boolean = !!opts.$list.$sort
    if (opts.$list.$find) {
      return parseFind(opts.$list.$find, ids, needsQeury)
    } else {
      if (!traverse) {
        return [{ isFork: true }, '$list without find needs traverse']
      } else {
        return parseFind(
          {
            $traverse: traverse
          },
          ids,
          needsQeury
        )
      }
    }
  } else if (opts.$find) {
    return [{ isFork: true }, 'Find outside of a list not supported']
  }
  return [{ isFork: true }, 'Not a valid query']
}

const parseQuery = (
  getField: GetFieldFn,
  getOptions: GetOptions,
  ids: string[],
  traverse?: string,
  language?: string,
  version?: string
): [GetResult[], string | null] => {
  const resultGet = {}
  const results: GetResult[] = []
  if (getOptions.$list && getOptions.$find) {
    return [results, 'If using $list put $find in list']
  }
  let resultIds: any[] | undefined = []
  let resultFork: Fork | undefined
  if (getOptions.$list || getOptions.$find) {
    const [r, err] = parseNested(getOptions, ids, traverse)
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
      resultIds = r
    }
  }
  if (resultFork) {
    const [q, err] = createSearchString(resultFork)
    const query: string = q.substring(1, q.length - 1)
    if (err) {
      return [results, err]
    }
    const args = createSearchArgs(getOptions, query, resultFork)
    logger.info(args)
    printAst(resultFork, args)
    const queryResult: string[] = redis.call('ft.search', 'default', ...args)
    resultIds = queryResult
  }

  if (resultIds) {
    const find = getFind(getOptions)
    if (find && find.$find) {
      if (getOptions.$list) {
        table.remove(resultIds, 1)
        // FIXME: nested sort and range
        if (getOptions.$list.$sort) {
          return [results, 'Nested find sort is not supported yet!']
        }
      }
      const opts: GetOptions = { id: true }
      for (let key in getOptions) {
        if (key !== '$find' && key !== '$list' && key !== '$id') {
          opts[key] = getOptions[key]
        }
      }
      opts.$list = { $find: find.$find }
      const [nestedResults, err] = parseQuery(
        getField,
        opts,
        resultIds,
        undefined,
        language,
        version
      )
      if (err) {
        return [results, err]
      }
      const nestedMap: Record<string, boolean> = {}
      for (let i = 0; i < nestedResults.length; i++) {
        const item = nestedResults[i]
        if (!nestedMap[item.id]) {
          nestedMap[item.id] = true
          if (!getOptions.id) {
            delete item.id
          }
          results[results.length] = item
        }
      }
    } else {
      const schema = getSchema()
      for (let i = 1; i < resultIds.length; i++) {
        const result: GetResult = {}
        getField(
          getOptions,
          schema,
          result,
          resultIds[i],
          '',
          language,
          version,
          '$'
        )
        results[results.length] = result
      }
    }
  }

  return [results, null]
}

const queryGet = (
  getField: GetFieldFn,
  result: GetResult,
  getOptions: GetOptions,
  resultField: string,
  ids?: string[],
  traverse?: string,
  language?: string,
  version?: string
): string | null => {
  if (!ids) {
    ids = [getOptions.$id || 'root']
  }
  let [r, err] = parseQuery(
    getField,
    getOptions,
    ids,
    traverse,
    language,
    version
  )
  if (!r.length || r.length === 0) {
    r = emptyArray()
  }
  result[resultField] = r
  if (err) {
    return err
  }
  return null
}

export default queryGet
