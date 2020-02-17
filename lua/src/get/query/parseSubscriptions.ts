import { FilterAST, Meta, QuerySubscription, Fork } from './types'
import * as logger from '../../logger'
import { isFork } from './util'
import { getPrefixFromType } from '../../typeIdMapping'
import { indexOf, isArray, joinString } from '../../util'
import { GetOptions } from '~selva/get/types'
import createSearchString from './createSearchString'
import createSearchArgs from './createSearchArgs'

const addType = (type: string | number, arr: string[]) => {
  const prefix = getPrefixFromType(tostring(type))
  if (indexOf(arr, prefix) === -1) {
    arr[arr.length] = prefix
  }
}

function parseFork(
  ast: Fork,
  sub: QuerySubscription,
  invertedAst: Fork,
  timestampFilters: FilterAST[]
) {
  let list: (Fork | FilterAST)[] = []
  let invertedAstList: (Fork | FilterAST)[] = []
  if (ast.$and) {
    list = ast.$and
    invertedAst.$and = invertedAstList = list = ast.$and
  } else if (ast.$or) {
    list = ast.$or
    invertedAst.$or = invertedAstList
  }

  if (list) {
    for (let i = 0; i < list.length; i++) {
      const item = list[i]
      if (isFork(item)) {
        const inverted: Fork = { isFork: true }
        invertedAstList[i] = inverted
        parseFork(item, sub, inverted, timestampFilters)
      } else {
        let inverted = item
        if (item.$field === 'type') {
          if (!sub.type) {
            sub.type = []
          }
          // FIXME: Not completely correct unfortunately
          // -- how to deal with ors?
          if (isArray(item.$value)) {
            for (let j = 0; j < item.$value.length; j++) {
              addType(item.$value[j], sub.type)
            }
          } else {
            addType(item.$value, sub.type)
          }
        } else if (item.$field === 'ancestors') {
          const ancestors: { $field: string; $value: string[] } = {
            $field: 'ancestors',
            $value: []
          }

          const value = !isArray(item.$value) ? [item.$value] : item.$value

          for (let j = 0; j < value.length; j++) {
            ancestors.$value[ancestors.$value.length] = tostring(value[j])
          }

          sub.member[sub.member.length] = ancestors
          // add to member
        } else if (item.$field === 'id') {
          if (!sub.ids) {
            sub.ids = {}
          }

          const value = !isArray(item.$value) ? [item.$value] : item.$value
          for (let j = 0; j < value.length; j++) {
            sub.ids[value[j]] = true
          }
        } else if (item.hasNow) {
          timestampFilters[timestampFilters.length] = item

          // invert ast change
          inverted = {
            $field: item.$field,
            $operator:
              item.$operator === '<'
                ? '>'
                : item.$operator === '>'
                ? '<'
                : item.$operator,
            $value: item.$value,
            $search: item.$search,
            hasNow: true
          }

          sub.fields[item.$field] = true
          // dont even know what to do here :D
          // prob need to add the traverse options and not the ids
        } else {
          sub.fields[item.$field] = true
        }

        invertedAstList[i] = inverted
      }
    }
  }
}

const parseGet = (
  opts: GetOptions,
  fields: Record<string, true>,
  field: string[]
) => {
  for (let key in opts) {
    if (key[0] !== '$') {
      const item = opts[key]
      if (typeof item === 'object') {
        const newArray: string[] = []
        for (let i = 0; i < field.length; i++) {
          newArray[newArray.length] = field[i]
        }
        newArray[newArray.length] = key
        parseGet(item, fields, newArray)
      } else {
        fields[
          field.length > 0 ? joinString(field, '.') + '.' + key : key
        ] = true
      }
    }
  }
}

function parseSubscriptions(
  querySubs: QuerySubscription[],
  meta: Meta,
  ids: string[],
  getOptions: GetOptions,
  traverse?: string | string[]
) {
  logger.info('PARSE SUBSCRIPTIONS')
  let sub: QuerySubscription | undefined

  // FIXME: prob better to just do an isEqual check
  const queryId = redis.sha1hex(cjson.encode(getOptions))

  for (let i = 0; i < querySubs.length; i++) {
    if (querySubs[i].queryId === queryId) {
      sub = querySubs[i]
      break
    }
  }
  if (!sub) {
    sub = {
      member: [],
      fields: {},
      queryId
    }
    querySubs[querySubs.length] = sub
  }

  parseGet(getOptions, sub.fields, [])

  // getOptions
  // recurse trough getOptions

  logger.info('META.AST', meta.ast)
  if (meta.ast) {
    const invertedAst: Fork = { isFork: meta.ast.isFork }
    const timestampFilters: FilterAST[] = []
    parseFork(meta.ast, sub, invertedAst, timestampFilters)
    logger.info('INVERTED', invertedAst)
    // TODO: the more I think about this,
    // the less senes query inversion actually starts to make
    // we could make it much more general if we just change all conditions to > where 'now' is used
    // and look up the closest item
    // that way we also don't really need to invert the conditions and don't have to worry
    // about nesting as much at least
    // or more complex logical operators
    // might end up invalidating too quickly, but at least it works
    // and can be optimized if we do a better job figuring out which field
    // is the important one, which we'd need to do anyways
    // TODO:
    // create search string from the inverted ast
    // create search args with sort by nearest time
    // range [0, 1]
    // INVERSE QUERY HERE
    if (timestampFilters.length >= 1) {
      const [invertedSearch] = createSearchString(invertedAst)
      // TODO: when multiple timestamp columns need to invert logical operator in their context
      // also need to do something about the sort in that case
      // like maybe we can just preserve whatever condition has > 'now'
      const invertedArgs = createSearchArgs(
        {
          $list: {
            $sort: {
              $field: timestampFilters[0].$field, // it's actually not so easy to decide which timestamp field should be the basis of sorting
              $order: 'asc'
            },
            $range: [0, 1]
          }
        },
        invertedSearch,
        invertedAst
      )
      logger.info('GET OPTIONS', getOptions)
      logger.info('INVERTED SEARCH STRING', invertedSearch)
      logger.info('SEARCH ARGS', invertedArgs)
      logger.info('TS FIELD', timestampFilters[0].$field)
      const invertedSearchResults: string[] = redis.call(
        'ft.search',
        'default',
        ...invertedArgs
      )
      logger.info('RESULTS', invertedSearchResults)
      const earliestId = invertedSearchResults[1]
      if (earliestId) {
        const time = redis.call('hget', earliestId, timestampFilters[0].$field)
        logger.info('NEXT TIMESTAMP', time)
      }
    }
  } else {
    // need to check if TYPE is there
    // no qeury on fields etc easy
    // if (!sub.ids) {
    //   sub.ids = {}
    // }
    // for (let i = 1; i < meta.ids.length; i++) {
    //   sub.ids[meta.ids[i]] = true
    // }
  }

  if (sub.ids || !meta.ast) {
    if (!sub.idFields) {
      sub.idFields = {}
    }
    const field = meta.traverse || traverse
    if (!field) {
      logger.error('WRONG MISSING FIELD or TRAVERSE')
    } else {
      for (let i = 0; i < ids.length; i++) {
        sub.idFields[`${ids[i]}.${field}`] = true
      }
    }
  }

  let sort = meta.sort
  if (sort) {
    if (!isArray(sort)) {
      sort = [sort]
    }
    for (let i = 0; i < sort.length; i++) {
      sub.fields[sort[i].$field] = true
    }
  }
}

export default parseSubscriptions
