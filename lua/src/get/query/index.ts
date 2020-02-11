import * as logger from '../../logger'
import { GetOptions, GetResult } from '~selva/get/types'
import createSearchString from './createSearchString'
import parseFind from './parseFind/index'
import createSearchArgs from './createSearchArgs'
import { Fork } from './types'
import printAst from './printAst'
import { isFork, getFind } from './util'
import { emptyArray, ensureArray } from '../../util'
import { GetFieldFn } from '../types'
import parseList from './parseList'
import { Schema } from '../../../../src/schema/index'

const parseNested = (
  opts: GetOptions,
  ids: string[],
  traverse?: string | string[]
): [Fork | string[], string | null] => {
  if (opts.$list) {
    if (opts.$list.$find) {
      if (!opts.$list.$find.$traverse) {
        opts.$list.$find.$traverse = traverse
      }

      return parseFind(opts.$list.$find, ids)
    } else {
      if (!traverse) {
        return [{ isFork: true }, '$list without find needs traverse']
      } else {
        return parseFind(
          {
            $fields: ensureArray(traverse)
          },
          ids
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
  schema: Schema,
  getOptions: GetOptions,
  ids: string[],
  traverse?: string | string[],
  language?: string,
  version?: string,
  includeMeta?: boolean
): [
  {
    results: GetResult[]
    meta?: { ast: Fork | undefined }
  },
  string | null
] => {
  const resultGet = {}
  const results: GetResult[] = []
  if (getOptions.$list && getOptions.$find) {
    return [{ results }, 'If using $list put $find in list']
  }
  let resultIds: any[] | undefined = []
  let resultFork: Fork | undefined
  if (getOptions.$list || getOptions.$find) {
    const [r, err] = parseNested(getOptions, ids, traverse)
    if (err) {
      return [{ results }, err]
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
      return [{ results }, err]
    }
    const args = createSearchArgs(getOptions, query, resultFork)
    printAst(resultFork, args)
    const queryResult: string[] = redis.call('ft.search', 'default', ...args)
    resultIds = queryResult
  } else if (getOptions.$list) {
    resultIds = parseList(resultIds, getOptions.$list)
    if (resultIds.length === 0) {
      resultIds = []
    }
  }

  if (resultIds) {
    const find = getFind(getOptions)

    if (find && find.$find) {
      // nested find
      if (getOptions.$list) {
        table.remove(resultIds, 1)
      }
      const opts: GetOptions = { id: true }
      for (let key in getOptions) {
        if (key !== '$find' && key !== '$list' && key !== '$id') {
          opts[key] = getOptions[key]
        }
      }
      opts.$list = {
        $find: find.$find
      }

      if (getOptions.$list && getOptions.$list.$sort) {
        opts.$list.$sort = getOptions.$list.$sort
      }

      if (getOptions.$list && getOptions.$list.$range) {
        opts.$list.$range = getOptions.$list.$range
      }

      // meta is harder here..

      const [{ results: nestedResults }, err] = parseQuery(
        getField,
        schema,
        opts,
        resultIds,
        undefined,
        language,
        version
      )
      if (err) {
        return [{ results }, err]
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
          includeMeta,
          '$'
        )
        results[results.length] = result
      }
    }
  }

  // need stuff for nested!!!
  // also for search
  // also for range
  return [{ results, meta: { ast: resultFork } }, null]
}

const queryGet = (
  getField: GetFieldFn,
  schema: Schema,
  result: GetResult,
  getOptions: GetOptions,
  resultField: string,
  ids?: string[],
  traverse?: string | string[],
  language?: string,
  version?: string,
  includeMeta?: boolean
): string | null => {
  if (!ids) {
    ids = [getOptions.$id || 'root']
  }

  const [r, err] = parseQuery(
    getField,
    schema,
    getOptions,
    ids,
    traverse,
    language,
    version,
    includeMeta
  )

  let { results, meta } = r

  if (!results.length || results.length === 0) {
    results = emptyArray()
  }
  if (includeMeta) {
    if (result.$meta) {
      result.$meta.query = meta
    } else {
      result.$meta = { query: meta }
    }
  }
  result[resultField] = results
  if (err) {
    return err
  }
  return null
}

export default queryGet
