import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'
import { SCRIPT } from '../constants'
import validate, { ExtraQueries } from './validate'

async function combineResults(
  client: SelvaClient,
  extraQueries: ExtraQueries,
  getResult: GetResult
) {
  if (Object.keys(extraQueries).length === 0) {
    return
  }

  if (Object.keys(getResult).length === 1 && getResult.listResult) {
    await Promise.all(
      getResult.listResult.map(res => {
        return combineResults(client, extraQueries, res)
      })
    )
    return
  }

  await Promise.all(
    Object.entries(extraQueries).map(async ([db, query]) => {
      await Promise.all(
        query.map(async q => {
          const parts = q.path.substr(1).split('.')

          if (parts[0] === 'listResult') {
            parts.shift()
          }

          let g = getResult
          for (let i = 0; i < parts.length - 2; i++) {
            const part = parts[i]

            if (!g[part]) {
              g[part] = {}
            }

            g = g[part]
          }

          if (q.type === 'reference') {
            q.getOpts.$id = g[parts[parts.length - 1]]
            const r = await get(client, q.getOpts)
            g[parts[parts.length - 1]] = r
          } else if (q.type === 'references') {
            if (q.getOpts.$list) {
              if (q.getOpts.$list === true) {
                q.getOpts.$list = {
                  $find: {
                    $traverse: g[parts[parts.length - 1]]
                  }
                }
              } else if (q.getOpts.$list.$find) {
                q.getOpts.$list.$find.$traverse = g[parts[parts.length - 1]]
              } else {
                // $list but no $find
                q.getOpts.$list.$find = {
                  $traverse: g[parts[parts.length - 1]]
                }
              }

              delete q.getOpts.$db
              const r = await client.get({ listResult: q.getOpts })
              if (r.listResult[0] && r.listResult[0].__$find) {
                console.log('HEYYOOOOOOOO', r.listResult[0].__$find)
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
                const db = findOpts.$db
                delete findOpts.$db
                console.log('FIRST RESPONSE', JSON.stringify(r, null, 2))
                console.log(
                  'UUUUUUUUH',
                  JSON.stringify(
                    {
                      $db: db,
                      listResult: {
                        ...fieldKeys,
                        $list: {
                          $find: { ...findOpts, $traverse: [...findIds] }
                        }
                      }
                    },
                    null,
                    2
                  )
                )
                const nestedResult = await client.get({
                  $db: db,
                  listResult: {
                    ...fieldKeys,
                    $list: {
                      $find: { ...findOpts, $traverse: [...findIds] }
                    }
                  }
                })
                console.log('OOOOOOOOO', nestedResult)
                g[parts[parts.length - 1]] = nestedResult.listResult
              } else {
                g[parts[parts.length - 1]] = r.listResult
              }
            } else if (q.getOpts.$find) {
              q.getOpts.$find.$traverse = g[parts[parts.length - 1]]
              delete q.getOpts.$db
              const r = await client.get({ listResult: q.getOpts })
              g[parts[parts.length - 1]] = r.listResult
            }
          } else {
            const r = await get(client, q.getOpts)
            g[parts[parts.length - 1]] = r
          }
        })
      )
    })
  )
}

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const extraQueries: ExtraQueries = {}
  await validate(extraQueries, client, props)

  const getResult = await client.redis.evalsha(
    { name: props.$db || 'default' },
    `${SCRIPT}:fetch`,
    0,
    `${this.loglevel}:${this.clientId}`,
    JSON.stringify(props)
  )

  await combineResults(client, extraQueries, getResult)
  return getResult
}

export { get, GetResult, GetOptions }
