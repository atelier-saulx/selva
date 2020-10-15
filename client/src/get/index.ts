import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'
import createGetOperations from './createGetOperations'
import executeGetOperations from './executeGetOperations'
import resolveId from './resolveId'
import combineResults from './combineResults'

// add this later!
import validate, {
  ExtraQueries,
  ExtraQuery,
  PostGetExtraQuery
} from './validate'

// move to saulx/utils
import { deepMerge } from '@saulx/utils'

function getExtraQueriesByField(
  extraQueries: ExtraQueries
): Record<string, ExtraQuery> {
  const map: Record<string, ExtraQuery> = {}
  for (const db in extraQueries) {
    for (const q of extraQueries[db]) {
      map[q.path] = q
    }
  }
  return map
}

function makeNewGetOptions(
  extraQueries: Record<string, ExtraQuery>,
  getOpts: GetOptions,
  path: string = ''
): GetOptions {
  if (Object.keys(extraQueries).length === 0) {
    return getOpts
  }

  const newOpts = {}
  for (const key in getOpts) {
    const newPath = path + '.' + key
    if (extraQueries[newPath]) {
      const extraQuery: ExtraQuery = extraQueries[newPath]
      if (extraQuery.type === 'traverse') {
        newOpts[key] = extraQuery.value || []
      } else if (extraQuery.type === 'text_search') {
        // TODO: in full
        // here we gotta somehow convert the text search results into a $traverse and/or filter and/or something that makes sense in our new query
        // TODO: add $db support? fun!
      } else {
        newOpts[key] = extraQuery.placeholder
      }
    } else if (
      !key.startsWith('$') &&
      key !== 'path' &&
      Array.isArray(getOpts[key])
    ) {
      newOpts[key] = getOpts[key].map((g, i) => {
        const extraQuery: PostGetExtraQuery = <PostGetExtraQuery>(
          extraQueries[newPath + '.' + i]
        )

        if (extraQuery) {
          return extraQuery.placeholder
        }

        return makeNewGetOptions(extraQueries, g, newPath + '.' + i)
      })
    } else if (Array.isArray(getOpts[key])) {
      newOpts[key] = getOpts[key]
    } else if (typeof getOpts[key] === 'object') {
      newOpts[key] = makeNewGetOptions(extraQueries, getOpts[key], newPath)
    } else {
      newOpts[key] = getOpts[key]
    }
  }

  return newOpts
}

async function get(
  client: SelvaClient,
  props: GetOptions,
  meta?: any,
  nested: boolean = false
): Promise<GetResult> {
  // -------------
  // // this is where we collect the dbs and  which initialize schemas to do after
  // TODO: need to intialize for each db!

  const db = props.$db || 'default'
  const subId = props.$subscription

  await client.initializeSchema({ db })

  const extraQueries: ExtraQueries = {}
  await validate(extraQueries, client, props)
  const newProps = makeNewGetOptions(
    getExtraQueriesByField(extraQueries),
    props
  )

  const id = await resolveId(client, newProps)

  if (!id) {
    return { $isNull: true }
  }

  const lang = newProps.$language

  const getResult = await executeGetOperations(
    client,
    lang,
    { db, subId },
    createGetOperations(client, newProps, id, '', db)
  )

  // maybe ncie function?
  if (meta || props.$includeMeta) {
    if (!meta) {
      if (!getResult.$meta) {
        getResult.$meta = {}
      }
      meta = { [props.$db || 'default']: getResult.$meta }
      meta.___refreshAt = getResult.$meta.___refreshAt
    } else {
      if (getResult.$meta.___refreshAt) {
        if (
          !meta.___refreshAt ||
          meta.___refreshAt > getResult.$meta.___refreshAt
        ) {
          meta.___refreshAt = getResult.$meta.___refreshAt
        }
      }
      deepMerge(meta, {
        [props.$db || 'default']: getResult.$meta
      })
      delete getResult.$meta
    }
  }

  await combineResults(client, extraQueries, props.$language, getResult, meta)

  if (props.$includeMeta && !nested) {
    getResult.$meta = meta
  }

  return getResult
}

export { get, GetResult, GetOptions }
