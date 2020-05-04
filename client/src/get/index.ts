import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'
import validate, { ExtraQueries } from './validate'

async function combineResults(
  _client: SelvaClient,
  extraQueries: ExtraQueries,
  getResult: GetResult
) {
  await Promise.all(
    Object.entries(extraQueries).map(async ([db, query]) => {
      const selva = global.SELVAS[db]
      await Promise.all(
        query.map(async q => {
          const parts = q.path.substr(1).split('.')
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
            const r = await selva.get(q.getOpts)
            g[parts[parts.length - 1]] = r
          } else if (q.type === 'references') {
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
              q.getOpts.$list.$find = { $traverse: g[parts[parts.length - 1]] }
            }

            delete q.getOpts.$db
            const r = await selva.get({ listResult: q.getOpts })
            g[parts[parts.length - 1]] = r.listResult
          }
        })
      )
    })
  )
}

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const extraQueries: ExtraQueries = {}
  await validate(extraQueries, client, props)
  const getResult = await client.fetch(props)
  await combineResults(client, extraQueries, getResult)
  return getResult
}

export { get, GetResult, GetOptions }
