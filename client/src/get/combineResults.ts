import { SelvaClient } from '../'
import { deepMerge } from '@saulx/utils'
// add this later!
import { ExtraQueries, ExtraQuery } from './validate'
import { GetResult } from './types'

// circ deps allways weird
import { get } from './'

export default async function combineResults(
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

  // TODO: Pass DB everywhere

  await Promise.all(
    Object.entries(extraQueries).map(async ([db, query]) => {
      await Promise.all(
        query.map(async q => {
          if (q.type === 'traverse' || q.type === 'text_search') {
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
                $db: db, // if not db ofc
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
