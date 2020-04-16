import * as logger from '../../logger'
import { GetOptions, GetResult, Sort } from '~selva/get/types'
import createSearchString from './createSearchString'
import parseFind from './parseFind/index'
import createSearchArgs from './createSearchArgs'
import { Fork, Meta, QuerySubscription } from './types'
import printAst from './printAst'
import { isFork, getFind } from './util'
import { emptyArray, ensureArray, isArray, splitString } from '../../util'
import { GetFieldFn } from '../types'
import parseList from './parseList'
import { Schema } from '../../../../src/schema/index'
import parseSubscriptions from './parseSubscriptions'
import { setNestedResult, setMeta } from '../nestedFields'

import globals from '../../globals'

const parseNested = (
  opts: GetOptions,
  ids: string[],
  meta: Meta,
  traverse?: string | string[]
): [Fork | string[], string | null] => {
  if (opts.$list) {
    if (typeof opts.$list === 'object' && opts.$list.$find) {
      if (!opts.$list.$find.$traverse) {
        opts.$list.$find.$traverse = traverse
      }
      return parseFind(opts.$list.$find, ids, meta)
    } else {
      if (!traverse) {
        return [{ isFork: true }, '$list without find needs traverse']
      } else {
        return parseFind(
          {
            $fields: ensureArray(traverse)
          },
          ids,
          meta
        )
      }
    }
  } else if (opts.$find) {
    // return [{ isFork: true }, 'Find outside of a list not supported']
    // TODO: disallow $range
    if (!opts.$find.$traverse) {
      opts.$find.$traverse = traverse
    }
    const result = parseFind(opts.$find, ids, meta)
    return result
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
  getResult?: GetResult
): [
  {
    results: GetResult[]
    meta?: Meta
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

  const meta: Meta = { ids: resultIds }

  if (getOptions.$list || getOptions.$find) {
    const [r, err] = parseNested(getOptions, ids, meta, traverse)
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
    const idMap: Record<string, true> = {}
    const [queries, err] = createSearchString(resultFork, language)
    if (queries.length === 1) {
      const query: string = queries[0]
      if (err) {
        return [{ results }, err]
      }

      const args = createSearchArgs(getOptions, query, resultFork)
      // printAst(resultFork, args)
      const queryResult: string[] = redis.call('ft.search', 'default', ...args)
      if (queryResult) {
        if (queries.length === 1) {
          table.remove(queryResult, 1)
          resultIds = queryResult
        }
      }
    } else {
      for (const q of queries) {
        if (err) {
          return [{ results }, err]
        }
        const args = createSearchArgs(
          getOptions,
          string.sub(q, 2, q.length - 1),
          resultFork
        )
        // printAst(resultFork, args)
        const queryResult: string[] = redis.call(
          'ft.search',
          'default',
          ...args
        )

        if (queryResult) {
          for (let i = 1; i < queryResult.length; i++) {
            idMap[queryResult[i]] = true
          }
        }
      }
    }

    if (
      getOptions.$list &&
      typeof getOptions.$list === 'object' &&
      (getOptions.$list.$limit ||
        getOptions.$list.$offset ||
        getOptions.$list.$sort)
    ) {
      for (const id in idMap) {
        resultIds[resultIds.length] = id
      }
      resultIds = parseList(resultIds, getOptions.$list)
    } else {
      for (const id in idMap) {
        resultIds[resultIds.length] = id
      }
    }
  } else if (getOptions.$list) {
    resultIds = parseList(resultIds, getOptions.$list)
    if (resultIds.length === 0) {
      resultIds = []
    }
  }

  if (resultIds) {
    const find = getFind(getOptions)
    // need to do something here for nested queries
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

      if (typeof getOptions.$list === 'object') {
        if (getOptions.$list && getOptions.$list.$sort) {
          opts.$list.$sort = getOptions.$list.$sort
        }

        if (getOptions.$list && getOptions.$list.$offset) {
          opts.$list.$offset = getOptions.$list.$offset
        }

        if (getOptions.$list && getOptions.$list.$limit) {
          opts.$list.$limit = getOptions.$list.$limit
        }
      }

      if (resultIds.length !== 0) {
        const [{ results: nestedResults }, err] = parseQuery(
          getField,
          schema,
          opts,
          resultIds,
          undefined,
          language,
          version,
          getResult
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
      }
    } else {
      // printAst(resultFork)

      const sort =
        getOptions.$list &&
        typeof getOptions.$list === 'object' &&
        getOptions.$list.$sort

      meta.ast = resultFork
      if (sort) {
        meta.sort = sort
      }
      // meta.ids = resultIds

      if (
        getOptions.$list &&
        typeof getOptions.$list === 'object' &&
        getOptions.$list.$find &&
        getOptions.$list.$find.$traverse
      ) {
        meta.traverse = getOptions.$list.$find.$traverse
      } else if (getOptions.$find && getOptions.$find.$traverse) {
        meta.traverse = getOptions.$find.$traverse
      } else if (traverse) {
        meta.traverse = traverse
      }

      if (resultFork) {
        // printAst(resultFork)
        // logger.info('hello', meta)

        let funObject: any
        if (globals.$meta) {
          const ballz: QuerySubscription[] = []
          const x: GetResult = {}

          parseSubscriptions(
            ballz,
            meta,
            resultIds,
            getOptions,
            x,
            language,
            traverse
          )

          const snurf = ballz[0]

          const fields = snurf.fields

          logger.info('flapperdrol', meta.traverse, snurf)

          if (meta.traverse === 'descendants') {
            if (snurf.member.length > 1) {
              logger.info('HOW CAN THIS BE MULTIPLE MEMBERS')
            }

            let members = snurf.member[0]

            const memberId = redis.sha1hex(cjson.encode(members)).substr(0, 10)

            setMeta(undefined, undefined, {
              ___contains: { [memberId]: members }
            })

            const type = snurf.type

            funObject = {}

            if (type) {
              funObject.___types = {}
              for (let i = 0; i < type.length; i++) {
                funObject.___types[type[i]] = { [memberId]: true }
              }
            } else {
              funObject.___any = {
                [memberId]: true
              }
            }

            for (let key in fields) {
              setMeta(key, funObject)
            }
          } else {
            // can choose to make this in member checks
            setMeta(meta.traverse, { ___ids: ids })
            for (let key in fields) {
              setMeta(key, { ___ids: resultIds })
            }
          }
        }

        for (let i = 0; i < resultIds.length; i++) {
          const r: GetResult = {}
          getField(
            getOptions,
            schema,
            r,
            resultIds[i],
            '',
            language,
            version,
            '$',
            meta.traverse === 'descendants' ? funObject : undefined
          )
          results[results.length] = r
        }
      } else {
        // logger.info('META', meta, 'ID', ids)
        // TODO: add sort fields with traverse id
        if (meta.traverse) {
          setMeta(meta.traverse, { ___ids: ids })
        }

        if (meta.sort) {
          for (const sort of ensureArray(meta.sort)) {
            setMeta(sort.$field, { ___ids: resultIds })
          }
        }

        for (let i = 0; i < resultIds.length; i++) {
          const r: GetResult = {}
          getField(
            getOptions,
            schema,
            r,
            resultIds[i],
            '',
            language,
            version,
            '$'
          )
          results[results.length] = r
        }
      }
    }
  }

  return [{ results, meta }, null]
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
  version?: string
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
    result
  )

  let { results, meta } = r

  if (!results.length || results.length === 0) {
    results = emptyArray()
  }

  // if (globals.$meta && meta) {
  //   // if (!result.$meta.query) {
  //   //   result.$meta.query = []
  //   // }
  //   parseSubscriptions(
  //     result.$meta.query,
  //     meta,
  //     ids,
  //     getOptions,
  //     result,
  //     language,
  //     traverse
  //   )
  // }

  if (getOptions.$find) {
    setNestedResult(result, resultField, results.length ? results[0] : {})
  } else {
    setNestedResult(result, resultField, results)
  }

  if (err) {
    logger.error(err)
    return err
  }
  return null
}

export default queryGet
