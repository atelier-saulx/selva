import { SchemaMutations } from '.'
import { SelvaClient } from '..'

export default async (
  client: SelvaClient,
  mutations: SchemaMutations,
  handleMutations: (old: { [field: string]: any }) => {
    [field: string]: any
  }
): Promise<void> => {
  console.info('????????????', mutations)
  const setObject: { [key: string]: any } = {}

  const gets = {}

  for (const f of mutations) {
    if (!gets[f.type]) {
      gets[f.type] = {
        id: true,
      }
    }

    let x = gets[f.type]
    for (let i = 0; i < f.path.length - 1; i++) {
      const p = f.path[i]
      if (!x[p]) {
        x[p] = {}
      }
      x = x[p]
    }
    x[f.path[f.path.length - 1]] = true
    // console.info(f.path)
  }

  for (const type in gets) {
    // delete if its a different field name...

    let page = 0

    // 5k

    const query = {
      nodes: {
        ...gets[type],
        $list: {
          $offset: page * 5000,
          $limit: 5000,
          $find: {
            $traverse: 'descendants',
            $filter: {
              $operator: '=',
              $field: 'type',
              $value: type,
            },
          },
        },
      },
    }

    const existing = await client.get(query)
    console.dir(existing, { depth: 10 })
    console.dir(query, { depth: 10 })
  }

  console.info(gets)

  // for (const mutation)
}
