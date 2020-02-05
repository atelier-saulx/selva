import * as logger from '../../logger'
import { GetOptions } from '~selva/get/types'
import createSearchString from './createSearchString'
import parseFind from './parseFind'
import createSearchArgs from './createSearchArgs'
import { Fork } from './types'
import printAst from './printAst'

const parseNested = (
  opts: GetOptions,
  id: string,
  field?: string
): [Fork, string | null] => {
  if (opts.$list) {
    if (opts.$list.$find) {
      return parseFind(opts.$list.$find, id, field)
    } else if (opts.$list.$sort) {
      return [{ isFork: true }, 'Sort without find not implemented yet!']
    }
    // same for range
  } else if (opts.$find) {
    return parseFind(opts.$find, id, field)
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

  let resultFork: Fork | undefined
  if (getOptions.$list || getOptions.$find) {
    const [fork, err] = parseNested(getOptions, id, field)
    if (err) {
      return [null, err]
    }
    for (let key in getOptions) {
      if (key !== '$list' && key !== '$find') {
        resultGet[key] = getOptions[key]
      }
    }
    resultFork = fork
  }

  if (resultFork) {
    const [q, err] = createSearchString(resultFork)
    const qeury: string = q.substring(1, q.length - 1)

    printAst(resultFork, qeury)

    if (err) {
      return [null, err]
    }

    const args = createSearchArgs(getOptions, qeury)

    const queryResult = redis.call('ft.search', 'default', ...args)

    logger.info(queryResult)
    // const r = await Promise.all(
    //   queryResult.slice(1).map((id: string) => {
    //     const opts = Object.assign({}, getOptions, { $id: id })
    //     return client.get(opts)
    //   })
    // )
    // return [queryResult, null]
  }
  return [[], null]
}

const queryGet = (getOptions: GetOptions): any => {
  const id = getOptions.$id || 'root'
  const [result, err] = parseQuery(getOptions, id)
  if (err) {
    logger.error(err)
  }
  return result
}

export default queryGet
