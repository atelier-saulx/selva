import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'
import { SCRIPT } from '../constants'
import validate, {
  ExtraQueries,
  ExtraQuery,
  PostGetExtraQuery
} from './validate'
import { deepMerge } from './deepMerge'

async function combineResults(
  client: SelvaClient,
  extraQueries: ExtraQueries,
  $language: string | undefined,
  getResult: GetResult,
  meta?: any
) {
  if (Object.keys(extraQueries).length === 0) {
    return
  }

  if (Object.keys(getResult).length === 1 && getResult.listResult) {
    await Promise.all(
      getResult.listResult.map(res => {
        return combineResults(client, extraQueries, $language, res, meta)
      })
    )
    return
  }

  await Promise.all(
    Object.entries(extraQueries).map(async ([db, query]) => {
      await Promise.all(
        query.map(async q => {
          if (q.type === 'traverse') {
            // these are processed before the main query
            if (meta) {
              deepMerge(meta, q.meta)
            }
            return
          }

          const parts = q.path.substr(1).split('.')

          if (parts[0] === 'listResult') {
            parts.shift()
          }

          let g = getResult
          for (let i = 0; i <= parts.length - 2; i++) {
            const part = parts[i]

            if (Array.isArray(g[part]) && isNaN(<any>parts[i + 1])) {
              const newQuery: ExtraQuery = {
                type: q.type,
                getOpts: q.getOpts,
                path: '.' + parts.slice(i + 1).join('.'),
                placeholder: q.placeholder
              }

              return Promise.all(
                g[part].map(r => {
                  return combineResults(
                    client,
                    { [db]: [newQuery] },
                    $language,
                    r,
                    meta
                  )
                })
              )
            }

            if (!g[part]) {
              g[part] = {}
            }

            g = g[part]
          }

          if (q.type === 'reference') {
            const r = await get(
              client,
              {
                $language,
                $id: g[parts[parts.length - 1]],
                $db: db,
                $includeMeta: !!meta,
                ...q.getOpts
              },
              meta,
              true
            )

            g[parts[parts.length - 1]] = r
          } else if (q.type === 'references') {
            if (q.getOpts.$list) {
              const { $db: _db, ...gopts } = q.getOpts
              const r = await get(
                client,
                {
                  $language,
                  $includeMeta: !!meta,
                  $db: db,
                  listResult: {
                    ...gopts,
                    $list: {
                      ...(gopts.$list === true ? {} : gopts.$list),
                      $find: {
                        ...(gopts.$list === true
                          ? {}
                          : gopts.$list.$find || {}),
                        $traverse: g[parts[parts.length - 1]]
                      }
                    }
                  }
                },
                meta,
                true
              )

              if (r.listResult[0] && r.listResult[0].__$find) {
                let fieldKeys = {}
                for (const key in q.getOpts) {
                  if (!key.startsWith('$') && key !== '__$find') {
                    fieldKeys[key] = q.getOpts[key]
                  }
                }

                const findOpts = r.listResult[0].__$find.opts
                const findIds = r.listResult.reduce((acc, e) => {
                  acc.add(...e.__$find.ids)
                  return acc
                }, new Set())
                const { $db: db, ...fopts } = findOpts
                const nestedResult = await get(
                  client,
                  {
                    $language,
                    $includeMeta: !!meta,
                    $db: db,
                    listResult: {
                      ...fieldKeys,
                      $list: {
                        $find: {
                          ...fopts,
                          $traverse: [...findIds]
                        }
                      }
                    }
                  },
                  meta,
                  true
                )

                g[parts[parts.length - 1]] = nestedResult.listResult
              } else {
                g[parts[parts.length - 1]] = r.listResult
              }
            } else if (q.getOpts.$find) {
              const { $db: _db, ...gopts } = q.getOpts
              const r = await get(
                client,
                {
                  $language,
                  $db: db,
                  $includeMeta: !!meta,
                  listResult: {
                    ...gopts,
                    $find: {
                      ...gopts.$find,
                      $traverse: g[parts[parts.length - 1]]
                    }
                  }
                },
                meta,
                true
              )
              g[parts[parts.length - 1]] = r.listResult
            }
          } else {
            const r = await get(
              client,
              {
                $language,
                $includeMeta: !!meta,
                $db: db,
                ...q.getOpts
              },
              meta,
              true
            )
            g[parts[parts.length - 1]] = r
          }
        })
      )
    })
  )
}

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
  const extraQueries: ExtraQueries = {}
  await validate(extraQueries, client, props)
  const newProps = makeNewGetOptions(
    getExtraQueriesByField(extraQueries),
    props
  )

  const getResult = JSON.parse(
    await client.redis.evalsha(
      { name: props.$db || 'default', type: 'replica' },
      `${SCRIPT}:fetch`,
      0,
      `${client.loglevel}:${client.uuid}`,
      JSON.stringify(newProps)
    )
  )

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
