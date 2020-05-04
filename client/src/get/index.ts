import { SelvaClient } from '..'
import { GetResult, GetOptions } from './types'
import validate, { ExtraQueries } from './validate'

async function get(client: SelvaClient, props: GetOptions): Promise<GetResult> {
  const extraQueries: ExtraQueries = {}
  await validate(extraQueries, client, props)
  console.log('EXTRA QUERIES', JSON.stringify(extraQueries, null, 2))
  const getResult = await client.fetch(props)

  await Promise.all(
    Object.entries(extraQueries).map(async ([db, query]) => {
      const selva = global.SELVAS[db]
      await Promise.all(
        query.map(async q => {
          const parts = q.path.substr(1).split('.')
          let g = getResult
          console.log('parts', parts, 'g', g)
          for (let i = 0; i < parts.length - 2; i++) {
            const part = parts[i]

            if (!g[part]) {
              g[part] = {}
            }

            g = g[part]
            console.log('g', g)
          }

          if (q.type === 'reference') {
            console.log('GG', g, g[parts[parts.length - 1]])
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

  return getResult
}

export { get, GetResult, GetOptions }
