import * as logger from '../../logger'
import { GetOptions, GetResult, Inherit } from '~selva/get/types'
import parseFind from './parseFind/index'
import { Fork, Meta, QuerySubscription } from './types'
import { isFork, getFind } from './util'
import { emptyArray, ensureArray, joinString, joinPaddedIds } from '../../util'
import { GetFieldFn } from '../types'
import parseList from './parseList'
import ast2rpn from './ast2rpn'
import { Schema } from '../../../../src/schema/index'
import parseSubscriptions from './parseSubscriptions'
import { setNestedResult, setMeta } from '../nestedFields'

import globals from '../../globals'

const parseNested = (
  getField: GetFieldFn,
  opts: GetOptions,
  ids: string[],
  meta: Meta,
  traverse?: string | string[]
): [Fork | string[], string | null] => {
  if (opts.$list) {
    let $inherit: Inherit | undefined = undefined
    if (typeof opts.$list === 'object' && opts.$list.$inherit) {
      $inherit = opts.$list.$inherit
    }

    if (typeof opts.$list === 'object' && opts.$list.$find) {
      if (!opts.$list.$find.$traverse) {
        opts.$list.$find.$traverse = traverse
      }
      const o: any = opts.$list.$find
      if ($inherit) {
        o.$inherit = $inherit
      }
      return parseFind(getField, o, ids, meta)
    } else {
      if (!traverse) {
        return [{ isFork: true }, '$list without find needs traverse']
      } else {
        const o: any = {
          $fields: ensureArray(traverse)
        }

        if ($inherit) {
          o.$inherit = $inherit
        }
        return parseFind(getField, o, ids, meta)
      }
    }
  } else if (opts.$find) {
    // return [{ isFork: true }, 'Find outside of a list not supported']
    // TODO: disallow $range
    if (!opts.$find.$traverse) {
      opts.$find.$traverse = traverse
    }
    const result = parseFind(getField, opts.$find, ids, meta)
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
    const [r, err] = parseNested(getField, getOptions, ids, meta, traverse)
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
    // @ts-ignore
    const $traverse: string =
      (getOptions.$list &&
        // @ts-ignore
        getOptions.$list.$find &&
        // @ts-ignore
        <string>getOptions.$list.$find.$traverse) ||
      <string>traverse
    logger.info(
      'search',
      `"${$traverse}"`,
      ids,
      resultFork,
      ast2rpn(resultFork, language)
    )

    const [findIn, searchArgs] = ast2rpn(resultFork, language)
    let queryResult: string[]
    if (findIn) {
      logger.info('finding matches in ids', findIn, joinPaddedIds(findIn))
      queryResult = redis.call(
        'selva.hierarchy.findIn',
        '___selva_hierarchy',
        joinPaddedIds(findIn),
        ...searchArgs
      )

      if (queryResult) {
        for (let i = 0; i < queryResult.length; i++) {
          idMap[queryResult[i]] = true
        }
      }
    } else {
      queryResult = redis.call(
        'selva.hierarchy.find',
        '___selva_hierarchy',
        'bfs',
        $traverse,
        joinPaddedIds(ids),
        ...searchArgs
      )
    }

    logger.info('search res:', queryResult)

    if (queryResult) {
      for (let i = 0; i < queryResult.length; i++) {
        idMap[queryResult[i]] = true
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
        // @ts-ignore
        meta.traverse = getOptions.$list.$find.$traverse
      } else if (getOptions.$find && getOptions.$find.$traverse) {
        // @ts-ignore
        meta.traverse = getOptions.$find.$traverse
      } else if (traverse) {
        meta.traverse = traverse
      }

      if (resultFork) {
        let getMeta: any
        if (globals.$meta) {
          const subMeta = parseSubscriptions(
            meta,
            resultIds,
            getOptions,
            language,
            traverse
          )

          if (subMeta.time && subMeta.time.nextRefresh) {
            setMeta(undefined, undefined, {
              ___refreshAt: subMeta.time.nextRefresh
            })
          }

          const fields = subMeta.fields

          if (meta.traverse === 'descendants') {
            if (subMeta.member.length > 1) {
              logger.info('HOW CAN THIS BE MULTIPLE MEMBERS IN FIND QUERY')
            }

            let members = subMeta.member[0]

            const memberId = redis.sha1hex(cjson.encode(members)).substr(0, 10)

            setMeta(undefined, undefined, {
              ___contains: { [memberId]: members }
            })

            const type = subMeta.type

            getMeta = {}

            if (type) {
              getMeta.___types = {}
              for (let i = 0; i < type.length; i++) {
                getMeta.___types[type[i]] = { [memberId]: true }
              }
            } else {
              getMeta.___any = {
                [memberId]: true
              }
            }

            for (let key in fields) {
              setMeta(key, getMeta)
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
            undefined,
            language,
            version,
            '$',
            meta.traverse === 'descendants' ? getMeta : undefined
          )
          results[results.length] = r
        }
      } else {
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
    ids = [<string>getOptions.$id || 'root']
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

  if ((!results.length || results.length === 0) && !getOptions.$find) {
    setNestedResult(result, resultField, emptyArray())

    if (err) {
      logger.error(err)
      return err
    }
    return null
  }

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
